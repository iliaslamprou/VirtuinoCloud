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
    Fields needed (Console → Fields): "esp32/temperature", "esp32/humidity"
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
// ─────────────────────────────────────────────────────────────────────────────

// DHT22 sensor — change pin to D4 on ESP8266, 2 on WiFiNINA boards
#define DHT_PIN  4
DHT dht(DHT_PIN, DHT22);

VirtuinoCloud cloud(API_KEY);

void setup() {
    Serial.begin(115200);
    dht.begin();
    cloud.setClientId("esp32-01");   // optional: name in My Virtuino World

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

    // beginWrite starts a new block. "esp32" is the path: it is joined to every
    // field below with a "/", so these go to esp32/temperature and esp32/humidity.
    // (Or call beginWrite() with no path and add() the full names.)
    cloud.beginWrite("esp32");

    // add() queues a field — does NOT send yet
    // publish=true → also push to MQTT broker for live dashboard widgets
    cloud.add("temperature", temperature, true);
    cloud.add("humidity",    humidity,    true);

    // Optional: set a shared timestamp for all fields in this block
    // cloud.add("time", "2024-06-15T14:30:00Z");

    // send() transmits all queued fields in ONE HTTP POST and resets the list
    bool ok = cloud.send();

    if (ok) {
        Serial.print("Uploaded — T: "); Serial.print(temperature, 1);
        Serial.print(" C  H: ");        Serial.print(humidity, 1); Serial.println(" %");
    } else {
        Serial.print("Upload failed — HTTP "); Serial.println(cloud.lastStatus());
    }

    delay(30000);   // upload every 30 seconds
}
