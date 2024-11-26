#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Camera model for ESP32 Wrover
#define CAMERA_MODEL_WROVER
#include "camera_pins.h"  // Adjust camera pins

// SD card SPI pins
#define SD_CS_PIN    5   // Chip Select Pin for SD Card

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing ESP32 Wrover...");

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Set resolution
  config.frame_size = FRAMESIZE_VGA;  // Can change to higher resolutions
  config.jpeg_quality = 12;           // Lower value = higher quality
  config.fb_count = 1;

  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera initialization failed!");
    while (1);
  }
  Serial.println("Camera initialized successfully!");

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed!");
    return;
  }
  Serial.println("SD Card initialized.");
}

void loop() {
  // Capture a picture
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    delay(2000);
    return;
  }

  // Create a unique file name
  String path = "/photo_" + String(millis()) + ".jpg";
  Serial.printf("Saving to: %s\n", path.c_str());

  // Write image to SD card
  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing!");
  } else {
    file.write(fb->buf, fb->len);
    Serial.println("Picture saved successfully!");
  }
  file.close();

  // Return frame buffer
  esp_camera_fb_return(fb);

  // Wait before taking another picture
  delay(5000);
}
