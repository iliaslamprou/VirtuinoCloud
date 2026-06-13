/*
  VirtuinoCloud — Example 03: Read latest value
  -----------------------------------------------
  Reads a relay state from the Virtuino dashboard and switches a GPIO pin.
  The dashboard toggle widget writes 0 or 1 to the "relay1" field.
  This sketch polls the field every 5 seconds and mirrors the state on a GPIO.

  SUPPORTED BOARDS
    ESP32     — any variant
    ESP8266   — NodeMCU, Wemos D1 Mini, etc.  (core >= 3.x)
    WiFiNINA  — Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040

  REQUIRED LIBRARIES
    VirtuinoCloud · ArduinoJson
    ArduinoHttpClient  (WiFiNINA boards only)

  CONSOLE SETUP
    Device field needed: "relay1"
    Dashboard: add a Toggle widget connected to field "relay1"

  RESULT struct — all members available after a successful read:
    r.ok          bool   — false if network error or field not found
    r.asFloat()   float  — e.g. 1.0
    r.asInt()     int    — e.g. 1  (use this for ON/OFF)
    r.asString()  String — e.g. "1"
    r.time        char[] — ISO 8601 timestamp e.g. "2024-06-15T14:30:00Z"
    r.asJson()    String — {"value":"1","time":"2024-06-15T14:30:00Z"}
*/

#include <WiFi.h>              // ESP32
// #include <ESP8266WiFi.h>    // ESP8266  — uncomment and comment the line above
// #include <WiFiNINA.h>       // WiFiNINA — uncomment and comment the line above
#include <VirtuinoCloud.h>

// ── Credentials ───────────────────────────────────────────────────────────────
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY  = "YOUR_API_KEY";
const char* DEVICE   = "YOUR_DEVICE_ID";
// ─────────────────────────────────────────────────────────────────────────────

// GPIO connected to relay module (HIGH = ON, LOW = OFF)
// Change to D1 on ESP8266, 2 on WiFiNINA boards
const int RELAY_PIN = 26;

VirtuinoCloud cloud(API_KEY);

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" connected");
}

void loop() {
    // Read the latest value of field "relay1" from the cloud
    VirtuinoResult r = cloud.read(DEVICE, "relay1");

    if (r.ok) {
        // asInt() returns 1 when the dashboard toggle is ON, 0 when OFF
        digitalWrite(RELAY_PIN, r.asInt() ? HIGH : LOW);

        Serial.print("relay1 = "); Serial.print(r.asString());
        Serial.print("  at ");     Serial.println(r.time);

        // r.asJson() → {"value":"1","time":"2024-06-15T14:30:00Z"}
        // Serial.println(r.asJson());
    } else {
        Serial.println("Read failed — check WiFi, API key and device/field names");
    }

    delay(5000);    // poll every 5 seconds
}
