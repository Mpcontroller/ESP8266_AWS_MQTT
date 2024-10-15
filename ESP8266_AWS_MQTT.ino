#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"        // Topic for general publishing
#define INCREMENT_TOPIC         "esp8266/increment"  // Topic for incrementing values
#define DECREMENT_TOPIC         "esp8266/decrement"  // Topic for decrementing values
#define SENSOR_TOPIC            "esp8266/sensor"     // Topic for sensor values
#define WIFI_TOPIC              "esp8266/wifi"       // New topic for receiving WiFi data

// Define the analog pin for the water level sensor (if needed)
#define WATER_SENSOR_PIN A0

WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net);

time_t now;
time_t nowish = 1510592825;

float incrementValue = 0; // Variable for incrementing value
float decrementValue = 100; // Variable for decrementing value
float sensorValue = 0; // Variable for sensor value (random values)

void NTPConnect(void) {
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Handle received data from the "esp8266/wifi" topic
  if (strcmp(topic, WIFI_TOPIC) == 0) {
    Serial.println("Received WiFi data from cloud:");
    // Process the data here, e.g., convert payload to a string
    String wifiData = "";
    for (int i = 0; i < length; i++) {
      wifiData += (char)payload[i];
    }
    Serial.println("WiFi Data: " + wifiData);
    
    // Take action based on the received WiFi data, e.g., changing WiFi settings, controlling LEDs, etc.
  }
}

void connectAWS() {
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  NTPConnect();

  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);

  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to increment, decrement, sensor, and WiFi topics
  client.subscribe(INCREMENT_TOPIC);
  client.subscribe(DECREMENT_TOPIC);
  client.subscribe(WIFI_TOPIC);  // Subscribe to the new "esp8266/wifi" topic
  Serial.println("AWS IoT Connected!");
}

void publishIncrement() {
  client.publish(INCREMENT_TOPIC, String(incrementValue).c_str());
}

void publishDecrement() {
  client.publish(DECREMENT_TOPIC, String(decrementValue).c_str());
}

void publishSensorValue() {
  sensorValue = random(0, 101);  // Random value between 0 and 100
  client.publish(SENSOR_TOPIC, String(sensorValue).c_str());
}

void publishWaterLevel() {
  float sensorValueRaw = analogRead(WATER_SENSOR_PIN); 
  float waterLevelPercentage = (sensorValueRaw / 1024.0) * 100.0; // Assuming 10-bit ADC
  client.publish(AWS_IOT_PUBLISH_TOPIC, String(waterLevelPercentage).c_str());
}

void setup() {
  Serial.begin(115200);
  connectAWS();
  pinMode(16, OUTPUT);
}

void loop() {
  incrementValue += 1; // Increment by 1 each loop
  decrementValue -= 1; // Decrement by 1 each loop

  if (incrementValue > 100) {
    incrementValue = 0;
  }

  if (decrementValue < 0) {
    decrementValue = 100;
  }

  Serial.print("Increment Value = ");
  Serial.print(incrementValue);
  Serial.print(", Decrement Value = ");
  Serial.print(decrementValue);
  Serial.print(", Random Sensor Value = ");
  Serial.print(sensorValue);
  Serial.print(", Water Level = ");
  Serial.println(analogRead(WATER_SENSOR_PIN));

  if (!client.connected()) {
    connectAWS();
  } else {
    client.loop();
    publishIncrement();
    publishDecrement();
    publishSensorValue();
    publishWaterLevel();
    delay(800);
  }
}
