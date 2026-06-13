/*
  VirtuinoCloud — Example 02: Block write (multiple fields, 1 HTTP request)
  --------------------------------------------------------------------------
  Reads temperature and humidity from a DHT22 sensor and uploads both fields
  together in a single POST request using beginWrite / add / send.

  Without block write, 2 fields would require 2 separate HTTP requests.
  With block write, any number of fields (up to 16) costs exactly 1 request.

  SUPPORTED BOARDS
    ESP32     — any variant
    ESP8266   — NodeMCU, Wemos D1 Mini, etc.  (core >= 3.x)
    WiFiNINA  — Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040

  REQUIRED LIBRARIES
    VirtuinoCloud · ArduinoJson · DHT sensor library by Adafruit
    ArduinoHttpClient  (WiFiNINA boards only)

  CONSOLE SETUP
    Device fields needed: "temperature", "humidity"
*/

#include <WiFi.h>              // ESP32
// #include <ESP8266WiFi.h>    // ESP8266  — uncomment and comment the line above
// #include <WiFiNINA.h>       // WiFiNINA — uncomment and comment the line above
#include <DHT.h>
#include <VirtuinoCloud.h>

// ── Credentials ───────────────────────────────────────────────────────────────
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY  = "YOUR_API_KEY";
const char* DEVICE   = "YOUR_DEVICE_ID";
// ─────────────────────────────────────────────────────────────────────────────

// DHT22 sensor — change pin to D4 on ESP8266, 2 on WiFiNINA boards
#define DHT_PIN  4
DHT dht(DHT_PIN, DHT22);

VirtuinoCloud cloud(API_KEY);

void setup() {
    Serial.begin(115200);
    dht.begin();

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" connected");
}

void loop() {
    float temperature = dht.readTemperature();  // degrees Celsius
    float humidity    = dht.readHumidity();     // percent

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT22 read failed — check wiring");
        delay(5000);
        return;
    }

    // beginWrite starts a new block for the given device
    cloud.beginWrite(DEVICE);

    // add() queues a field — does NOT send yet
    // publish=true → also push to MQTT broker for live dashboard widgets
    cloud.add("temperature", temperature, true);
    cloud.add("humidity",    humidity,    true);

    // Optional: set a shared timestamp for all fields in this block
    // cloud.add("time", "2024-06-15T14:30:00Z");

    // send() transmits all queued fields in ONE HTTP POST and resets the list
    bool ok = cloud.send();

    if (ok) {
        Serial.printf("Uploaded — T: %.1f°C  H: %.1f%%\n", temperature, humidity);
    } else {
        Serial.println("Upload failed");
    }

    delay(30000);   // upload every 30 seconds
}
