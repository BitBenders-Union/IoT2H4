#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN GPIO_NUM_2
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

// Web server on port 80
AsyncWebServer server(80);

// Parameter names for Wi-Fi credentials
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";

// File paths to save Wi-Fi credentials
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";

String ssid;
String pass;

void initLittleFS();
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
bool initWiFi();
void setupServer(bool isWiFiInitialized);

void setup() {
    Serial.begin(115200);
    initLittleFS();

    // Load saved Wi-Fi credentials
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);

    // Initialize DHT sensor
    dht.begin();
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
    dht.humidity().getSensor(&sensor);
    delayMS = max(sensor.min_delay / 1000, 2000); // Minimum delay between reads

    // Initialize Wi-Fi and set up the appropriate web server
    if (initWiFi()) {
        // Initialize server for Wi-Fi
        setupServer(true);
    } else {
        // Initialize server for AP mode
        setupServer(false);
    }

}

void loop() {
    // Read DHT data periodically for debugging
    delay(delayMS);

    sensors_event_t event;

    // Get temperature
    dht.temperature().getEvent(&event);
    if (!isnan(event.temperature)) {
        Serial.print("Temperature: ");
        Serial.print(event.temperature);
        Serial.println(" °C");
    } else {
        Serial.println("Failed to read temperature");
    }

    // Get humidity
    dht.humidity().getEvent(&event);
    if (!isnan(event.relative_humidity)) {
        Serial.print("Humidity: ");
        Serial.print(event.relative_humidity);
        Serial.println(" %");
    } else {
        Serial.println("Failed to read humidity");
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
void setupServer(bool isWiFiInitialized) {
    if (isWiFiInitialized) {
        // Serve the main page if Wi-Fi is connected
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/index.html", "text/html");
        });

        server.on("/dht", HTTP_GET, [](AsyncWebServerRequest *request) {
            sensors_event_t event;

            // Read temperature
            dht.temperature().getEvent(&event);
            String temperature = isnan(event.temperature) ? "null" : String(event.temperature, 1);

            // Read humidity
            dht.humidity().getEvent(&event);
            String humidity = isnan(event.relative_humidity) ? "null" : String(event.relative_humidity, 1);

            // Create JSON response
            String json = "{\"temperature\": " + temperature + ", \"humidity\": " + humidity + "}";
            request->send(200, "application/json", json);
        });

    } else {
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
            }
            request->send(200, "text/plain", "Saved. Restarting...");
            delay(1000);
            ESP.restart();
        });
    }

    server.begin();
}