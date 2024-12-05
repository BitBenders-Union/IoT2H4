#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include "time.h"

#define resetPin 0

// DS18B20 configuration
const int oneWireBus = 4; // GPIO where the DS18B20 is connected
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

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
const char *dataPath = "/data.csv";

String ssid;
String pass;
String mqtt;

void initLittleFS();
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
bool initWiFi();
void setupServer();
void reconnect();
String getFormattedDateTime();
bool WriteDataToCSV(String data);
void PostDataToServerFromFile();
void resetAPMode();
void IRAM_ATTR ISR_EnableAccessPointMode();

const char *mqtt_server = "192.168.0.153"; // Raspberry Pi IP
const int mqtt_port = 1883;                // Default MQTT port
const char *mqtt_user = "mosquitto";       // Optional: Your MQTT username
const char *mqtt_pass = "dietpi";          // Optional: Your MQTT password
const char *mqtt_client_id = "ds18b20";    // Client ID for this ESP32

bool dataInFile = false;
bool noMqtt = false;

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;     // Copenhagen time zone (UTC+1)
const int daylightOffset_sec = 3600; // Daylight saving time (UTC+2 during summer)

RTC_DATA_ATTR bool triggerAPMode = false;

void setup()
{
    Serial.begin(115200);
    initLittleFS();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
    {
        resetAPMode();
    }

    // Load saved Wi-Fi credentials
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    mqtt = readFile(LittleFS, mqttPath);
    mqtt_server = mqtt.c_str();

    // Initialize DS18B20 sensor
    sensors.begin();

    if (!initWiFi())
    {
        setupServer();
    }

    // Setup MQTT
    client.setServer(mqtt_server, mqtt_port);

    // Initial connection to MQTT
    reconnect();

    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

     esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW); // Wake on LOW signal

    // Configure the timer as a wake-up source
    esp_sleep_enable_timer_wakeup(60 * 1000000); // 60 seconds in microseconds

    attachInterrupt(digitalPinToInterrupt(0), ISR_EnableAccessPointMode, LOW);
}

void loop()
{
    if (triggerAPMode)
    {
        resetAPMode();
    }
    // If there is data to send from file, do so
    if (dataInFile)
    {
        //Try commenting out the PostDataToServerFromFile function in the second code and see if the MQTT connection works consistently.
        //If commenting out the function resolves the issue, consider optimizing the file processing or implementing a mechanism to send data in smaller chunks.
        
        PostDataToServerFromFile();
    }

    // Ensure the MQTT connection is alive
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    // Read temperature from the DS18B20 sensor
    sensors.requestTemperatures();
    float temperatureValue = sensors.getTempCByIndex(0);

    // Validate sensor readings
    if (temperatureValue == DEVICE_DISCONNECTED_C)
    {
        Serial.println("Failed to read from DS18B20 sensor!");
        return;
    }

    // Convert reading to String with one decimal point
    String temperature = String(temperatureValue, 1);

    // Log the readings
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");

    // Log the current time
    String currentTime = getFormattedDateTime();
    Serial.println("Time: " + currentTime);

    // Create JSON payload to send
    String payload = "{\"temperature\": " + temperature +
                    ", \"time\": \"" + currentTime + "\"}";

    // If Wi-Fi is not connected, write data to CSV
    if (noMqtt)
    {
        String data = temperature + "," + currentTime;
        WriteDataToCSV(data);
        return;
    }

    // Publish data to MQTT broker
    if (client.publish("home/ds18b20", payload.c_str()))
    {
        Serial.println("Data sent to MQTT broker");
    }
    else
    {
        Serial.println("Failed to send data to MQTT broker");
    }
    //Disconnecting mqtt
    Serial.println("Disconncting mqtt..");
    client.disconnect();
    delay(500);
    // Put ESP32 to deep sleep for 1 minute (60 seconds)
    Serial.println("Going to sleep for 60 seconds...");

    esp_deep_sleep_start();
}

// Initialize LittleFS
void initLittleFS()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("Failed to mount LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // Create default files if they do not exist
    if (!LittleFS.exists(ssidPath))
    {
        writeFile(LittleFS, ssidPath, "");
    }
    if (!LittleFS.exists(passPath))
    {
        writeFile(LittleFS, passPath, "");
    }
    if (!LittleFS.exists(mqttPath))
    {
        writeFile(LittleFS, mqttPath, "");
    }
    if (!LittleFS.exists(dataPath))
    {
        writeFile(LittleFS, dataPath, "Temperature,Time\n");
    }
}

// Read a file from LittleFS
String readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);
    File file = fs.open(path, "r");
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return String();
    }
    String content = file.readStringUntil('\n');
    file.close();
    return content;
}

// Write a file to LittleFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);
    File file = fs.open(path, "w");
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(message);
    file.close();
}

// Initialize Wi-Fi
bool initWiFi()
{
    if (ssid.isEmpty())
    {
        Serial.println("Undefined SSID");
        return false;
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to Wi-Fi...");

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startAttemptTime > 10000)
        {
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
void setupServer()
{
    WiFi.softAP("ESP-WIFI-MANAGER");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Serve the Wi-Fi manager page if Wi-Fi is not connected
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/wifimanager.html", "text/html"); });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
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
      else if (p->name() == PARAM_INPUT_3) {
        mqtt = p->value();
        writeFile(LittleFS, mqttPath, mqtt.c_str());
      }
    }
    request->send(200, "text/plain", "Saved. Restarting...");
    delay(1000);
    ESP.restart(); });

    server.begin();
}

// MQTT reconnect function
void reconnect()
{
    int retries = 0;
    while (!client.connected())
    {
        if (client.connect(mqtt_client_id, mqtt_user, mqtt_pass))
        {
            Serial.println("Connected to MQTT broker");
        }
        else
        {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            retries++;
            if (retries > 3)
            {
                noMqtt = true;
                break;
            }
            delay(5000);
        }
    }
}

// Get formatted date and time
String getFormattedDateTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return "";
    }

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

// Write data to CSV file
bool WriteDataToCSV(String data)
{
    File file = LittleFS.open(dataPath, "a");
    if (!file)
    {
        Serial.println("Failed to open data file");
        return false;
    }
    file.println(data);
    file.close();
    dataInFile = true;
    return true;
}

// Send data from file to MQTT
void PostDataToServerFromFile()
{
    File file = LittleFS.open(dataPath, "r");
    if (!file)
    {
        Serial.println("Failed to open data file");
        return;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        if (!line.startsWith("Temperature")) // Skip header line
        {
            String payload = "{\"temperature\": " + line.substring(0, line.indexOf(',')) +
                            ", \"time\": \"" + line.substring(line.indexOf(',') + 1) + "\"}";
            if (client.publish("home/ds18b20", payload.c_str()))
            {
                Serial.println("Data sent: " + payload);
            }
        }
    }

    file.close();
    dataInFile = false;
}

void resetAPMode()
{
    triggerAPMode = false;
    // delete ssid file.
    LittleFS.remove(ssidPath);
    LittleFS.remove(passPath);
    LittleFS.remove(mqttPath);

    // restart the device
    ESP.restart();
}

void IRAM_ATTR ISR_EnableAccessPointMode()
{
    triggerAPMode = true;
}
