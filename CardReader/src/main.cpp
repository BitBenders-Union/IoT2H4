#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "time.h"

#define RST_PIN 22 // Reset pin for RFID
#define SS_PIN 5   // SDA pin for RFID
#define WIFI_CREDENTIALS_FILE "/wifi_credentials.txt"
#define uS_TO_S_FACTOR 1000000ULL // Conversion factor for microseconds to seconds

// WiFi and MQTT
String ssid;
String password;
const char* mqttServer = "192.168.0.153";
const int mqttPort = 1883;
const char* mqttTopic = "esp32/door_operations";
const char* mqttUser = "mosquitto";
const char* mqttPassword = "dietpi";

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);

MFRC522 mfrc522(SS_PIN, RST_PIN);

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
struct tm timeinfo;

unsigned long lastSentTime = 0; // Timestamp of the last sent message
const unsigned long messageInterval = 3000; // 3 seconds interval in milliseconds

// Function declarations
void startConfigPortal();
void wiFiSetup();
bool connectToMQTT();
void publishDoorOperation();
void checkAndSleep();
void handleRFID();

void setup() {
    Serial.begin(115200);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");

    SPI.begin(18, 19, 23); // Initialize RFID SPI
    mfrc522.PCD_Init();

    wiFiSetup();

    client.setServer(mqttServer, mqttPort);

    // Sync time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Print current time
    if (getLocalTime(&timeinfo)) {
        Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("Failed to obtain time.");
    }
}

void loop() {
    handleRFID();
    checkAndSleep();
}

// Handle RFID card detection
void handleRFID() {
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    // Check if enough time has passed since the last message
    if (millis() - lastSentTime > messageInterval) {
        // Publish the door operation with a timestamp
        if (connectToMQTT()) {
            publishDoorOperation();
        }
        lastSentTime = millis(); // Update the last sent time
    }

    mfrc522.PICC_HaltA();
}

// Publish door operation to MQTT with timestamp and event count (always 1)
void publishDoorOperation() {
    if (getLocalTime(&timeinfo)) {
        char timestamp[20];
        int door_count = 1;
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

        String payload = String("{\"time\":\"") + timestamp + "\",\"door_event\":" + String(door_count) + "}";
        client.publish(mqttTopic, payload.c_str());
        Serial.println("MQTT message sent: " + payload);
    } else {
        Serial.println("Failed to get time for MQTT message.");
    }
}

// WiFi setup with Config Portal
void wiFiSetup() {
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, FILE_READ);
    if (file) {
        ssid = file.readStringUntil('\n');
        password = file.readStringUntil('\n');
        ssid.trim();
        password.trim();
        file.close();
    }

    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());

        // Wait for connection
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(1000);
            Serial.println("Connecting to WiFi...");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected.");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            return;
        }
    }

    // If Wi-Fi not connected, start config portal
    startConfigPortal();
}

// Start WiFi configuration portal
void startConfigPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Config");

    IPAddress ip = WiFi.softAPIP(); // Get the IP address of the Access Point
    Serial.println("Configuration portal started.");
    Serial.print("Connect to the WiFi network 'ESP32-Config' and configure WiFi at: http://");
    Serial.println(ip); // Print the IP address to Serial Monitor

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/wifimanager.html", "text/html"); // Serve your HTML file
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest* request) {
        String newSSID, newPassword;
        if (request->hasParam("ssid", true)) {
            newSSID = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("password", true)) {
            newPassword = request->getParam("password", true)->value();
        }

        File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, FILE_WRITE);
        if (file) {
            file.println(newSSID);
            file.println(newPassword);
            file.close();
            request->send(200, "text/plain", "Credentials saved. Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to save credentials");
        }
    });

    server.begin();
    while (true) {
        delay(1000); // Keep the portal running until ESP restarts
    }
}

// Connect to MQTT broker
bool connectToMQTT() {
    if (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        while (!client.connected()) {
            if (client.connect("ESP32DoorCounter", mqttUser, mqttPassword)) {
                Serial.println("Connected to MQTT.");
            } else {
                Serial.println("Failed to connect to MQTT. Retrying in 5 seconds...");
                delay(5000);
                return false;
            }
        }
    }
    return true;
}

// Check time and sleep if necessary
void checkAndSleep() {
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time.");
        return;
    }

    if (timeinfo.tm_hour >= 17 || timeinfo.tm_hour < 6) {
        if (connectToMQTT()) {
            publishDoorOperation();
        }

        int secondsToSleep;
        if (timeinfo.tm_hour >= 17) {
            secondsToSleep = (24 - timeinfo.tm_hour + 6) * 3600 - timeinfo.tm_min * 60 - timeinfo.tm_sec;
        } else {
            secondsToSleep = (6 - timeinfo.tm_hour) * 3600 - timeinfo.tm_min * 60 - timeinfo.tm_sec;
        }

        Serial.printf("Going to sleep for %d seconds...\n", secondsToSleep);
        esp_sleep_enable_timer_wakeup((uint64_t)secondsToSleep * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
    }
}
