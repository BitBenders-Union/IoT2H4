#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN 15
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

// Web server on port 80
AsyncWebServer server(80);

// Parameter names for Wi-Fi credentials
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "mqtt";

// File paths to save credentials
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *mqttPath = "/mqtt.txt";

String ssid;
String pass;
String mqtt;

void initLittleFS();
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
bool initWiFi();
void setupServer();
void reconnect();

const char* mqtt_server = "192.168.0.153";  // Raspberry Pi IP
const int mqtt_port = 1883;                 // Default MQTT port
const char* mqtt_user = "mosquitto";         // Optional: Your MQTT username
const char* mqtt_pass = "dietpi";         // Optional: Your MQTT password
const char* mqtt_client_id = "dht-11"; // Client ID for this ESP32

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"pool.ntp.org", 36000, 60000);



void setup() {
    Serial.begin(115200);
    initLittleFS();

    // Load saved Wi-Fi credentials
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    mqtt = readFile(LittleFS, mqttPath);
    mqtt_server = mqtt.c_str();

    // Initialize DHT sensor
    dht.begin();
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
    dht.humidity().getSensor(&sensor);
    delayMS = max(sensor.min_delay / 1000, 2000);

    if (!initWiFi()){
        setupServer();
    }

    // Setup MQTT
    client.setServer(mqtt_server, mqtt_port);

    // Initial connection to MQTT
    reconnect();

    timeClient.begin();

}


void loop() {
    delay(delayMS);
    timeClient.update();

    // Ensure the MQTT connection is alive
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // Read temperature and humidity from the DHT sensor
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    float temperatureValue = event.temperature;

    dht.humidity().getEvent(&event);
    float humidityValue = event.relative_humidity;

    // Validate sensor readings
    if (isnan(temperatureValue) || isnan(humidityValue)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    // Convert readings to String with one decimal point
    String temperature = String(temperatureValue, 1);
    String humidity = String(humidityValue, 1);

    // Log the readings
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C, Humidity: ");
    Serial.print(humidity);
    Serial.println("%");

    // Log the current time
    String currentTime = timeClient.getFormattedTime();
    Serial.println("Time: " + currentTime);

    // Create JSON payload to send
    String payload = "{\"temperature\": " + temperature + ", \"humidity\": " + humidity + ", \"time\": \"" + currentTime + "\"}";

    // Publish data to MQTT broker
    if (client.publish("home/dht", payload.c_str())) {
        Serial.println("Data sent to MQTT broker");
    } else {
        Serial.println("Failed to send data to MQTT broker");
    }

}

// Initialize LittleFS
void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // Create default files if they do not exist
    if (!LittleFS.exists(ssidPath)) {
        writeFile(LittleFS, ssidPath, "");
    }
    if (!LittleFS.exists(passPath)) {
        writeFile(LittleFS, passPath, "");
    }
    if(!LittleFS.exists(mqttPath)){
        writeFile(LittleFS, mqttPath, "");
    }
}

// Read a file from LittleFS
String readFile(fs::FS &fs, const char *path) {
    Serial.printf("Reading file: %s\n", path);
    File file = fs.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return String();
    }
    String content = file.readStringUntil('\n');
    file.close();
    return content;
}

// Write a file to LittleFS
void writeFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Writing file: %s\n", path);
    File file = fs.open(path, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(message);
    file.close();
}

// Initialize Wi-Fi
bool initWiFi() {
    if (ssid.isEmpty()) {
        Serial.println("Undefined SSID");
        return false;
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to Wi-Fi...");

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttemptTime > 10000) {
            Serial.println("Failed to connect to Wi-Fi");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to Wi-Fi");
    Serial.println(WiFi.localIP());
    return true;
}

// Configure the server to serve the correct page
void setupServer() {
        WiFi.softAP("ESP-WIFI-MANAGER");
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);

        // Serve the Wi-Fi manager page if Wi-Fi is not connected
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/wifimanager.html", "text/html");
        });

        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
            int params = request->params();
            for (int i = 0; i < params; i++) {
                AsyncWebParameter *p = request->getParam(i);
                if (p->name() == PARAM_INPUT_1) {
                    ssid = p->value();
                    writeFile(LittleFS, ssidPath, ssid.c_str());
                } else if (p->name() == PARAM_INPUT_2) {
                    pass = p->value();
                    writeFile(LittleFS, passPath, pass.c_str());
                }
                else if (p->name() == PARAM_INPUT_3)
                {
                    mqtt = p->value();
                    writeFile(LittleFS, mqttPath, mqtt.c_str());
                }
                
            }
            request->send(200, "text/plain", "Saved. Restarting...");
            delay(1000);
            ESP.restart();
        });

    server.begin();
    while(true){
        delay(1000);
    }
}

// MQTT reconnect function
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

