#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// WiFi credentials
const char* sta_ssid     = "Lukrasta";
const char* sta_password = "Cycy12345";

const char* ap_ssid      = "CO2-Meter";
const char* ap_password  = "12345678";

// ThingSpeak
const char* tsWriteKey  = "GCV42MXJVS4KS87H";
const char* tsServerURL = "http://api.thingspeak.com/update";

// Web server (hotspot page)
WebServer server(80);

// I2C + OLED settings
#define SDA_PIN 8
#define SCL_PIN 10
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NeoPixel ring
#define LED_PIN 4
#define NUMPIXELS 12
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// SGP30 Sensor
Adafruit_SGP30 sgp30;
int16_t co2ppm = 400;

// Hotspot web page handler
void handleRoot() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>CO2 Monitor</title>";
  page += "<meta http-equiv='refresh' content='5'>";
  page += "<style>";
  page += "body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;background-color:#f0f4f8;color:#333;}";
  page += "h1{font-size:120px;margin:20px 0;color:#0070C0;}";
  page += "h2{font-size:55px;margin:10px 0;}";
  page += "p{font-size:28px;margin-top:20px;}";
  page += "</style>";
  page += "</head><body>";
  page += "<h2>CO2 Monitor</h2>";
  page += "<h1>" + String(co2ppm) + " ppm</h1>";
  page += "<p>Updated every 5 seconds</p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Setup
void setup() {
  Serial.begin(115200);
  delay(1000);

  // OLED init
  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting CO2 Meter...");
  display.display();

  // NeoPixel init
  pixels.begin();
  pixels.setBrightness(5);
  pixels.fill(pixels.Color(255, 255, 255));
  pixels.show();

  // SGP30 init
  delay(1000);
  if (!sgp30.begin()) {
    Serial.println("SGP30 not found!");
    display.println("SGP30 not found!");
    display.display();
    while (1);
  }
  Serial.print("SGP30 serial # ");
  Serial.print(sgp30.serialnumber[0], HEX);
  Serial.print(sgp30.serialnumber[1], HEX);
  Serial.println(sgp30.serialnumber[2], HEX);

  // Warm-up
  display.println("SGP30 warming up...");
  display.display();
  delay(15000);
  Serial.println("SGP30 ready");

  // WiFi dual mode
  WiFi.mode(WIFI_AP_STA);

  // 1. Create own hotspot (CO2-Meter)
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Hotspot started. AP IP: ");
  Serial.println(WiFi.softAPIP());
  display.println("Hotspot: CO2-Meter");
  display.display();

  // 2. Join Lukrasta
  WiFi.begin(sta_ssid, sta_password);
  Serial.print("Connecting to Lukrasta");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Lukrasta!");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    display.println("WiFi: Lukrasta OK");
    display.print("IP: ");
    display.println(WiFi.localIP());
  } else {
    Serial.println("\nCould not connect to Lukrasta.");
    display.println("Lukrasta: failed");
  }
  display.display();

  // Hotspot web server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");
}

// Loop
void loop() {
  server.handleClient();

  // SGP30 measurement
  if (sgp30.IAQmeasure()) {
    co2ppm = sgp30.eCO2;
  } else {
    Serial.println("SGP30 measurement failed");
  }

  Serial.printf("eCO2: %d ppm\n", co2ppm);

  // NeoPixel color mapping
  uint8_t r = constrain(map(co2ppm, 800, 2000, 0, 255), 0, 255);
  uint8_t g = constrain(map(co2ppm, 800, 2000, 255, 0), 0, 255);
  pixels.fill(pixels.Color(r, g, 0));
  pixels.show();

  // OLED display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("eCO2 Level:");
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(co2ppm);
  display.println(" ppm");
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();

  // POST to ThingSpeak
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(tsServerURL) +
                 "?api_key=" + tsWriteKey +
                 "&field1="  + String(co2ppm);

    http.begin(url);
    int httpResponseCode = http.GET();
    Serial.print("ThingSpeak response: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      Serial.println(http.getString());
    } else {
      Serial.println("POST failed — check Wi-Fi and ThingSpeak key");
    }
    http.end();
  } else {
    Serial.println("WiFi lost — skipping ThingSpeak update");
  }

  delay(5000);
}