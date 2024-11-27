#include <Arduino.h>
#include <UltrasonicSensor.h>
#include <WiFi.h>
#include "time.h"

#define trigPin 13
#define echoPin 14
#define MAX_DISTANCE 700

#define uS_TO_S_FACTOR 1000000  // Conversion factor for microseconds to seconds

// Wi-Fi credentials
const char* ssid = "E308";
const char* password = "98806829";

// Time settings for Danish time (CET)
const char* ntpServer = "pool.ntp.org"; // NTP server
const long gmtOffset_sec = 3600;       // GMT+1 for Denmark
const int daylightOffset_sec = 3600;

float timeOut = MAX_DISTANCE * 60;
int soundVelocity = 340;

struct tm timeinfo;


float getSonar();
void checkAndSleep();

void setup() {
    Serial.begin(115200);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

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
    // delay(1000);
    //   if (!getLocalTime(&timeinfo)) {
    //   Serial.println("Failed to obtain time.");
    //   return;
    // }

    // Print current time for debugging
    Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);


}

void loop() {
    checkAndSleep();
    delay(1000);
    Serial.printf("Distance: %.2f cm\n", getSonar());
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

void checkAndSleep() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time.");
    return;
  }

  // Print current time for debugging
  Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // Check if it's time to sleep
  if (timeinfo.tm_hour >= 17 || timeinfo.tm_hour < 6) {
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