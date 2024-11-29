#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "time.h"
#include "FS.h"
#include "SPIFFS.h"

#define trigPin 13
#define echoPin 14
#define MAX_DISTANCE 700

#define uS_TO_S_FACTOR 1000000  // Conversion factor for microseconds to seconds

// Wi-Fi credentials
const char* ssid = "E308";
const char* password = "98806829";

// MQTT settings
const char* mqttServer = "192.168.0.153"; // Replace with your broker if needed
const int mqttPort = 1883;
const char* mqttTopic = "esp32/people_counter";
const char* mqttUser = "mosquitto";   // MQTT username
const char* mqttPassword = "dietpi";  // MQTT password

WiFiClient espClient;
PubSubClient client(espClient);

// Time settings for Danish time (CET)
const char* ntpServer = "pool.ntp.org"; // NTP server
const long gmtOffset_sec = 3600;       // GMT+1 for Denmark
const int daylightOffset_sec = 3600;

float timeOut = MAX_DISTANCE * 60;
int soundVelocity = 340;

struct tm timeinfo;

float getSonar();
void checkAndSleep();
void logPersonDetection();
void sendCSVData();
bool connectToMQTT();
void saveToCSV(String timestamp);

void setup() {
    Serial.begin(115200);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("WiFi connected.");

    // Print the ESP32's IP address
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    // Synchronize time using NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // MQTT setup
    client.setServer(mqttServer, mqttPort);

    // Initial time fetch
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time.");
    } else {
        Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

void loop() {
    float distance = getSonar();

    // Check if a person is detected
    if (distance > 0 && distance < 200) { // Adjust threshold for your application
        logPersonDetection();
    }

    delay(1000);
    checkAndSleep();
}

float getSonar() {
    unsigned long pingTime;
    float distance;

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    pingTime = pulseIn(echoPin, HIGH, timeOut);
    distance = (float)pingTime * soundVelocity / 2 / 10000;

    return distance;
}

void logPersonDetection() {
    if (getLocalTime(&timeinfo)) {
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

        // Save to SPIFFS
        saveToCSV(String(timestamp));

        // Send MQTT message
        String payload = String("{\"time\":\"") + timestamp + "\",\"persons\":1}";
        if (connectToMQTT()) {
            client.publish(mqttTopic, payload.c_str());
            Serial.println("MQTT message sent: " + payload);
        }
    }
}

void saveToCSV(String timestamp) {
    File file = SPIFFS.open("/people_data.csv", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }

    // Append new data
    String data = timestamp + ",1\n";
    if (file.print(data)) {
        Serial.println("Data appended successfully: " + data);
    } else {
        Serial.println("Failed to append data");
    }
    file.close();
}

void sendCSVData() {
    if (connectToMQTT()) {
        File file = SPIFFS.open("/people_data.csv", FILE_READ);
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        Serial.println("Sending CSV data...");
        while (file.available()) {
            String line = file.readStringUntil('\n');
            client.publish(mqttTopic, line.c_str());
            Serial.println("Sent line: " + line);
        }
        file.close();

        // Clear the file after sending
        SPIFFS.remove("/people_data.csv");
    }
}

bool connectToMQTT() {
    if (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        while (!client.connected()) {
            if (client.connect("ESP32PeopleCounter", mqttUser, mqttPassword)) { // Use credentials
                Serial.println("Connected to MQTT.");
            } else {
                Serial.print("Failed MQTT connection. Retrying in 5 seconds...");
                delay(5000);
            }
        }
    }
    return client.connected();
}

void checkAndSleep() {
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time.");
        return;
    }

    // Print current time for debugging
    Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Check if it's time to sleep
    if (timeinfo.tm_hour >= 17 || timeinfo.tm_hour < 6) {
        // Send all collected CSV data before sleeping
        sendCSVData();

        // Calculate the time to sleep until 06:00
        int secondsToSleep;
        if (timeinfo.tm_hour >= 17) {
            // From 17:00 to midnight
            secondsToSleep = (24 - timeinfo.tm_hour + 6) * 3600 - timeinfo.tm_min * 60 - timeinfo.tm_sec;
        } else {
            // From midnight to 06:00
            secondsToSleep = (6 - timeinfo.tm_hour) * 3600 - timeinfo.tm_min * 60 - timeinfo.tm_sec;
        }

        Serial.printf("Going to sleep for %d seconds until 06:00...\n", secondsToSleep);
        esp_sleep_enable_timer_wakeup((uint64_t)secondsToSleep * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
    }
}
