#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <FastBot.h>


#define MY_PERIOD 60000  
#define wifi_ssid "*"
#define wifi_password "*"

#define mqtt_server "192.168.0.12" 
#define mqtt_user "*"
#define mqtt_password "*"

#define water_topic "sensor/water"
#define level_topic "sensor/level"

int analogRainPin = A0;  
int digitalRainPin = 12;  
uint32_t tmr1; 

const int MAX_SENSOR_READINGS = 10;
int sensorReadings[MAX_SENSOR_READINGS] = {0};
int sensorMidReadings[MAX_SENSOR_READINGS] = {0};
int lastMedium = 0;
int newMedium = 0;


#define CHAT_ID "*"
#define BOTtoken "*"  
FastBot bot(BOTtoken);


WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
float lvl = 0.0;
float wtr = 0.0;    

/**
 * Returns the readings from the water level sensor.
 * @return a message containing the sensor value and a normal range indication
 */
String getReadings(){
  int sensorValue = analogRead(analogRainPin);
  String message = "Значение датчика уровня воды: " + String(sensorValue) + " (норма < 300)";
  return message;
}

/**
 * Calculates the medium value of an array of sensor readings.
 * @return The median value of the sensor readings.
 */
int getMedium(){
  int sum = 0; 
  int medium = 0; 
  for ( int i = 0; i < 10; i++ ) {
      sum += sensorReadings[i];
  }
  medium = sum / 10;
  return medium;
}

/**
 * Retrieves the warnings from the sensor readings and returns the level of water.
 * @return The level of water as a string.
 */
String getWarnings(){
    for (int i = 0; i < MAX_SENSOR_READINGS; i++) {
      sensorReadings[i % MAX_SENSOR_READINGS] = analogRead(analogRainPin); 
    }
    String level = "Значение уровня воды: " + String(getMedium()) + " (норма < 300)";
    lastMedium = getMedium();
    Serial.println(lastMedium);
    return level;
}

/**
 * Retrieves the digital readings from the digitalRainPin and returns a message
 * indicating whether there is water present or not.
 *
 * @return A string indicating the presence of water based on the digital readings.
 *         Possible values are "Значение датчика капель: Нет воды." if there is no water,
 *         or "Значение датчика капель: Есть вода." if there is water.
 */
String getDigitalReadings(){
  int sensorValue = digitalRead(digitalRainPin);
  String message = "Значение датчика капель: Нет воды.";
  if (sensorValue < 1) {
    message = "Значение датчика капель: Есть вода.";
  }
  return message;
}

/**
 * Handles a new message received by the FB_msg object.
 * @param msg the FB_msg object containing the new message
 */
void newMsg(FB_msg& msg) {
  Serial.println("handleNewMessages");
    String chat_id = String(msg.chatID);

    if (chat_id != CHAT_ID) {
      Serial.println("CHAT_ID Error");
    }
    
    String text = msg.text;
    Serial.println(text);

    String from_name = msg.username;

    if (text == "/start") {
      String welcome = "Привет, " + from_name + ".\n";
      welcome += "Используйте эти команды для получения данных.\n\n";
      welcome += "/start \n";
      welcome += "/water \n";
            welcome += "/level \n";
      bot.sendMessage(welcome, chat_id);
    }

    if (text == "/water" || "/water@raino_meter_bot") {
      String dread = getDigitalReadings();
      bot.sendMessage(dread, chat_id);
    }  

    if (text == "/level" || "/level@raino_meter_bot") {
      String dread = getReadings();
      bot.sendMessage(dread, chat_id);
    }  
  
}

/**
 * Initializes the setup for the program.
 */
void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  bot.attach(newMsg);
}

/**
 * Function to set up Wi-Fi connection.
 */
void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting Wi-Fi...");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() > 15000) ESP.restart();
  }

  Serial.println("");
  Serial.println("Connected:");
  Serial.println(WiFi.localIP());
}

/**
 * Function to reconnect the client to MQTT if not already connected.
 */
void reconnect() {

  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)){
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * The loop function is responsible for executing the main logic of the program.
 * It checks if the client is connected to the MQTT broker and reconnects if necessary.
 * It publishes the current level and water detection status to the MQTT broker at regular intervals.
 * It updates the Telegram bot and sends warnings if the water level is increasing.
 *
 * @return void
 */
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();

  if (now - lastMsg > 5000) { 
     lastMsg = now;

    float lvl = analogRead(analogRainPin);
    float wtr = digitalRead(digitalRainPin);
    client.publish(level_topic, String(lvl).c_str(), true);
    client.publish(water_topic, String(wtr).c_str(), true);
  }

  bot.tick();

  if (millis() - tmr1 >= MY_PERIOD) { 
      tmr1 = millis(); 
    if (analogRead(analogRainPin) > 300 && digitalRead(digitalRainPin) < 1) {
        String warnings = "Внимание! Наблюдается повышение уровня воды, проверяем. \n";
        String warningsAll = getWarnings();
        String warningsDgt = getDigitalReadings();
        warnings += "Текущие данные с датчиков: \n\n";
        warnings += warningsAll + "\n";
        warnings += warningsDgt + "\n";
            if (lastMedium > newMedium) { 
                warnings += "Уровень повышается!";
            } else {
                warnings += "Уровень держится: наблюдаем.";
            }  
            bot.sendMessage(warnings, CHAT_ID);   
            newMedium = lastMedium;
            lastMedium = 0;
    }
  }
}