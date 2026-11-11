/*
 * Air Quality Sensor Node
 * ESP32 + MQ135 gas sensor + 0.96" SSD1306 OLED (I2C)
 *
 * Reads the MQ135 analog output, classifies the air quality,
 * shows it on the OLED, and serves the latest reading over Wi-Fi.
 *
 * Wiring:
 *   MQ135 AOUT -> GPIO34
 *   OLED  SDA  -> GPIO21
 *   OLED  SCL  -> GPIO22
 *   OLED  VCC  -> 3.3V
 *   MQ135 VCC  -> 5V
 *
 * Libraries (Arduino Library Manager):
 *   Adafruit SSD1306
 *   Adafruit GFX Library
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- Wi-Fi settings: replace with your network ----
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- Pins ----
const int MQ135_PIN = 34;   // ADC1, safe to use while Wi-Fi is on
const int SDA_PIN   = 21;
const int SCL_PIN   = 22;

// ---- Display ----
const int SCREEN_WIDTH  = 128;
const int SCREEN_HEIGHT = 64;
const uint8_t OLED_ADDR = 0x3C;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- Thresholds on the raw 12-bit reading (0 to 4095) ----
// Adjust these after letting the sensor warm up in clean air.
const int GOOD_MAX     = 3000;
const int MODERATE_MAX = 3600;

const unsigned long READ_INTERVAL_MS = 1000;
const int SAMPLES = 10;

WebServer server(80);
int airValue = 0;
unsigned long lastRead = 0;

const char* statusFor(int value) {
  if (value <= GOOD_MAX)     return "GOOD";
  if (value <= MODERATE_MAX) return "MODERATE";
  return "POOR";
}

int readSensor() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(MQ135_PIN);
    delay(5);
  }
  return sum / SAMPLES;
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Air Quality Monitor");

  display.setTextSize(2);
  display.setCursor(0, 22);
  display.println(airValue);

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Status: ");
  display.println(statusFor(airValue));

  display.display();
}

void handleRoot() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta http-equiv='refresh' content='2'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>Air Quality Node</title></head><body>";
  page += "<h2>Air Quality Monitor</h2>";
  page += "<p>Reading: " + String(airValue) + "</p>";
  page += "<p>Status: " + String(statusFor(airValue)) + "</p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

void handleData() {
  String json = "{\"value\":" + String(airValue) +
                ",\"status\":\"" + String(statusFor(airValue)) + "\"}";
  server.send(200, "application/json", json);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. Open http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi not available. Running with the display only.");
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found. Check the wiring and I2C address.");
  }
  display.clearDisplay();
  display.display();

  connectWiFi();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastRead >= READ_INTERVAL_MS) {
    lastRead = millis();
    airValue = readSensor();
    updateDisplay();
    Serial.printf("Reading: %d  Status: %s\n", airValue, statusFor(airValue));
  }
}
