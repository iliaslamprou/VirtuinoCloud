/*
  VirtuinoCloud — Example 01: Write single field
  ------------------------------------------------
  Uploads one sensor value to Virtuino Cloud every 30 seconds.

  SUPPORTED BOARDS
    ESP32        — select any ESP32 board in Tools → Board
    ESP8266      — NodeMCU, Wemos D1 Mini, etc.  (core >= 3.x required)
    WiFiNINA     — Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040

  REQUIRED LIBRARIES  (Sketch → Include Library → Manage Libraries)
    VirtuinoCloud
    ArduinoJson             by Benoit Blanchon
    ArduinoHttpClient       by Arduino  (WiFiNINA boards only)

  CONSOLE SETUP  (do this before uploading)
    1. Log in at virtuino.com
    2. Console → Fields → create a field named "esp32/temperature"
    3. Console → API & Connections → copy your API key
    4. Fill in SSID, PASSWORD and API_KEY below
*/

#include <WiFi.h>              // ESP32
// #include <ESP8266WiFi.h>    // ESP8266  — uncomment and comment the line above
// #include <WiFiNINA.h>       // WiFiNINA — uncomment and comment the line above
#include <VirtuinoCloud.h>

// ── Credentials — fill these in ──────────────────────────────────────────────
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY  = "YOUR_API_KEY";    // Console → API & Connections
// ─────────────────────────────────────────────────────────────────────────────

VirtuinoCloud cloud(API_KEY);

void setup() {
    Serial.begin(115200);

    // Optional: the name this board gets in My Virtuino World
    cloud.setClientId("esp32-01");

    // Connect to WiFi
    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" connected");
}

void loop() {
    float temperature = 23.4;   // replace with a real sensor read

    // Basic write — the field's full name, exactly as in Console → Fields.
    // The server records the time of arrival as timestamp.
    bool ok = cloud.write("esp32/temperature", temperature);

    // With publish=true the value is also pushed to the MQTT broker,
    // so live dashboard widgets update in real time (Essential+ plan)
    // bool ok = cloud.write("esp32/temperature", temperature, true);

    // With an explicit ISO 8601 UTC timestamp (useful for batch/offline uploads)
    // bool ok = cloud.write("esp32/temperature", temperature, true, "2024-06-15T14:30:00Z");

    if (ok) {
        Serial.print("Uploaded: "); Serial.println(temperature);
    } else {
        // lastStatus(): 404 = no field with that name, 403 = read-only or wrong API key,
        // 0 or negative = no connection
        Serial.print("Upload failed — HTTP "); Serial.println(cloud.lastStatus());
    }

    delay(30000);   // upload every 30 seconds
}
