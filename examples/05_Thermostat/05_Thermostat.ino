/*
  VirtuinoCloud — Example 05: Thermostat (Read + Block Write)
  ------------------------------------------------------------
  Demonstrates using read() and beginWrite/add/send together in one loop:

    1. Read "setpoint" from the dashboard (slider widget sets the target temp)
    2. Measure actual temperature with a DHT22
    3. Control a heater relay using bang-bang logic with 0.5°C hysteresis
    4. Upload "temperature" and "heater_state" in ONE HTTP request

  DASHBOARD SETUP  (Console → Devices → your device → add these fields)
    setpoint     — written by a Slider widget  (range 15–30)
    temperature  — displayed on a Gauge or Chart widget
    heater_state — displayed on an LED widget  (0 = OFF, 1 = ON)

  BANG-BANG LOGIC
    heater turns ON  when actual < setpoint − 0.5°C
    heater turns OFF when actual > setpoint + 0.5°C
    no change in the 1°C deadband around the setpoint (prevents rapid switching)

  SUPPORTED BOARDS
    ESP32     — any variant
    ESP8266   — NodeMCU, Wemos D1 Mini, etc.  (core >= 3.x)
    WiFiNINA  — Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040

  REQUIRED LIBRARIES
    VirtuinoCloud · ArduinoJson · DHT sensor library by Adafruit
    ArduinoHttpClient  (WiFiNINA boards only)
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

// Hardware pins — adjust for your board:
//   ESP8266:  DHT_PIN = D4,  HEATER = D1
//   WiFiNINA: DHT_PIN = 2,   HEATER = 4
const int DHT_PIN = 4;    // DHT22 data pin
const int HEATER  = 26;   // relay module controlling the heater (HIGH = ON)

DHT dht(DHT_PIN, DHT22);
VirtuinoCloud cloud(API_KEY);

void setup() {
    Serial.begin(115200);
    dht.begin();
    pinMode(HEATER, OUTPUT);

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" connected");
}

void loop() {
    // ── Step 1: read setpoint from dashboard ─────────────────────────────────
    // If the read fails (no WiFi, field not found, etc.) use 22°C as default
    VirtuinoResult sp = cloud.read(DEVICE, "setpoint");
    float setpoint = sp.ok ? sp.asFloat() : 22.0f;

    // ── Step 2: measure actual temperature ───────────────────────────────────
    float actual = dht.readTemperature();
    if (isnan(actual)) {
        Serial.println("DHT22 read failed — check wiring");
        delay(5000);
        return;
    }

    // ── Step 3: bang-bang control with hysteresis ─────────────────────────────
    bool heaterOn = digitalRead(HEATER);    // keep current state if in deadband
    if (actual < setpoint - 0.5f) heaterOn = true;    // too cold  → ON
    if (actual > setpoint + 0.5f) heaterOn = false;   // warm enough → OFF
    digitalWrite(HEATER, heaterOn ? HIGH : LOW);

    // ── Step 4: upload both measurements in one HTTP request ──────────────────
    // publish=true → values appear immediately on live dashboard widgets
    cloud.beginWrite(DEVICE);
    cloud.add("temperature",  actual,                true);
    cloud.add("heater_state", heaterOn ? 1.0f : 0.0f, true);
    bool ok = cloud.send();

    Serial.printf("%s  SP: %.1f°C  Actual: %.1f°C  Heater: %s\n",
                  ok ? "OK  " : "FAIL",
                  setpoint, actual,
                  heaterOn ? "ON" : "OFF");

    delay(10000);   // run control loop every 10 seconds
}
