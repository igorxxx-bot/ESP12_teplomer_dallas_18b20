//ESP12F modul

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <time.h>

#include <my_settings.h>

#define ONE_WIRE_BUS 12   // DQ na IO12
#define ONE_WIRE_VDD 14   // napájení Dallasu na IO14

#define SERVICE_PIN 4     // D3 (GPIO0) ... LOW = OTA update

#define SLEEP_TIME_SEC 600  // 10 minut

// ----- GLOBAL -----
WiFiClient espClient;
PubSubClient mqttClient(espClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempAddresses[10]; // max 10 dallas
uint8_t sensorCount = 0;

// ----- TIME CONFIG -----
const long gmtOffset_sec = 3600;      
const int daylightOffset_sec = 3600;   

// ----- FUNCTIONS -----
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }
}

void setupOTA() {
  ArduinoOTA.setHostname(WiFi.macAddress().c_str());
  ArduinoOTA.begin();
}

void setupMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
  if (mqttUser[0] != '\0') {
    mqttClient.connect(WiFi.macAddress().c_str(), mqttUser, mqttPassword);
  } else {
    mqttClient.connect(WiFi.macAddress().c_str());
  }
}

void publishTemperature() {
  sensors.requestTemperatures(); 
  for (uint8_t i = 0; i < sensorCount; i++) {
    float tempC = sensors.getTempC(tempAddresses[i]);
    char addrStr[17] = {0};
    for (uint8_t j = 0; j < 8; j++) {
      sprintf(addrStr + j*2, "%02X", tempAddresses[i][j]);
    }

    String topic = "esp/" + WiFi.macAddress() + "/dallas/" + String(addrStr) + "/temperature";
    char payload[8];
    dtostrf(tempC, 4, 2, payload);
    mqttClient.publish(topic.c_str(), payload, true);
  }
}

void publishTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    String topic = "esp/" + WiFi.macAddress() + "/time";
    mqttClient.publish(topic.c_str(), buf, true);
  }
}

void publishRSSI() {
  long rssi = WiFi.RSSI();
  String topic = "esp/" + WiFi.macAddress() + "/rssi";
  mqttClient.publish(topic.c_str(), String(rssi).c_str(), true);
}

void scanSensors() {
  sensors.begin();
  sensors.setResolution(12);
  sensorCount = sensors.getDeviceCount();
  for (uint8_t i = 0; i < sensorCount; i++) {
    sensors.getAddress(tempAddresses[i], i);
  }
}

void deepSleep() {
//  sensors.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  ESP.deepSleep(SLEEP_TIME_SEC * 1000000, WAKE_RF_DEFAULT);
}

void setup() {
  pinMode(SERVICE_PIN, INPUT_PULLUP);

  // Servisní režim OTA
  if (digitalRead(SERVICE_PIN) == LOW) {
    Serial.begin(115200);
    setupWiFi();
    setupOTA();
    while (true) {
      ArduinoOTA.handle();
      delay(100);
    }
  }

  pinMode(ONE_WIRE_VDD, OUTPUT);
  digitalWrite(ONE_WIRE_VDD, HIGH);

  setupWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

  scanSensors();
  sensors.requestTemperatures(); 

  setupMQTT();

  publishTemperature();
  publishTime();
  publishRSSI();

  mqttClient.disconnect();
  deepSleep();
}

void loop() {
  // no need
}
