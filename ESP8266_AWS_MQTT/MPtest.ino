#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

// Define the analog pin for the water level sensor
#define WATER_SENSOR_PIN A0

WiFiClientSecure net;

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

PubSubClient client(net);

time_t now;
time_t nowish = 1510592825;

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

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

void publishMessage(float waterLevelPercentage) {
    char messageBuffer[8];  // Buffer to hold the water level percentage as a string
  dtostrf(waterLevelPercentage, 6, 2, messageBuffer);  // Convert float to string (6 digits, 2 decimal places)

  client.publish(AWS_IOT_PUBLISH_TOPIC, messageBuffer);  // Publish the string valu
}

void setup() {
  Serial.begin(115200);
  connectAWS();
}

void loop() {
  float waterLevelPercentage = readWaterLevelPercentage(); // Get water level in percentage

  Serial.print("Water Level = ");
  Serial.print(waterLevelPercentage);
  Serial.println(" %"); // Water level in percentage

  now = time(nullptr);

  if (!client.connected()) {
    connectAWS();
  } else {
    client.loop();
    publishMessage(waterLevelPercentage);
    delay(1000); // Publish every 5 seconds
  }
}

// Function to read water level from the sensor in percentage
float readWaterLevelPercentage() {
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  float percentage = map(sensorValue, 0, 1023, 0, 100);  // Map the sensor value to a percentage (0-100)
  return percentage;
}
