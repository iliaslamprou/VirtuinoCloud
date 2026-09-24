/*
  VirtuinoCloud — Example 04: Read history
  ------------------------------------------
  Fetches the last 50 records of a field as a JSON array String,
  then parses the array with ArduinoJson to print each entry and
  calculate the average value.

  readHistory() returns a String in this format (newest first):
    [
      {"time":"2024-06-15T14:30:00Z","value":"23.4","source":"HTTP"},
      {"time":"2024-06-15T14:29:00Z","value":"23.1","source":"HTTP"},
      ...
    ]
  Returns "[]" on network error or if the field has no data yet.

  MEMORY NOTE
    Each record takes ~100 bytes of heap for the JSON document.
    Keep count ≤ 50 on ESP8266 (limited to ~80 KB free heap).
    ESP32 can handle up to 500+ records comfortably.
    Server maximum: 5000 records.

  SUPPORTED BOARDS
    ESP32     — any variant
    ESP8266   — NodeMCU, Wemos D1 Mini, etc.  (core >= 3.x)
    WiFiNINA  — Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040

  REQUIRED LIBRARIES
    VirtuinoCloud · ArduinoJson
    ArduinoHttpClient  (WiFiNINA boards only)

  CONSOLE SETUP
    Field needed (Console → Fields): "esp32/temperature"
    Upload some values first (e.g. using Example 01) so history exists.
*/

#include <WiFi.h>              // ESP32
// #include <ESP8266WiFi.h>    // ESP8266  — uncomment and comment the line above
// #include <WiFiNINA.h>       // WiFiNINA — uncomment and comment the line above
#include <ArduinoJson.h>       // needed here to parse the returned array
#include <VirtuinoCloud.h>

// ── Credentials ───────────────────────────────────────────────────────────────
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY  = "YOUR_API_KEY";
// ─────────────────────────────────────────────────────────────────────────────

VirtuinoCloud cloud(API_KEY);
bool fetched = false;   // fetch only once on startup

void setup() {
    Serial.begin(115200);

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" connected — fetching history...");
}

void loop() {
    if (!fetched) {
        // Fetch the last 50 records of the field "esp32/temperature"
        String history = cloud.readHistory("esp32/temperature", 50);

        // Parse the JSON array with ArduinoJson
        // Allocate enough memory: ~100 bytes × number of records
        DynamicJsonDocument doc(8192);
        DeserializationError err = deserializeJson(doc, history);

        if (err) {
            Serial.print("JSON parse error: "); Serial.println(err.c_str());
        } else {
            JsonArray arr = doc.as<JsonArray>();
            float sum = 0;
            int   n   = 0;

            for (JsonObject rec : arr) {
                const char* ts    = rec["time"]   | "unknown";
                const char* val   = rec["value"]  | "0";
                const char* src   = rec["source"] | "";
                Serial.print("["); Serial.print(n); Serial.print("]  ");
                Serial.print(ts);  Serial.print("  ");
                Serial.print(val); Serial.print("  ("); Serial.print(src); Serial.println(")");
                sum += atof(val);
                n++;
            }

            // Summary
            if (n > 0) {
                Serial.println("─────────────────────────────────");
                Serial.print("Records: "); Serial.print(n);
                Serial.print("   Average: "); Serial.println(sum / n, 2);
            } else if (cloud.lastStatus() != 200) {
                Serial.print("Read failed — HTTP "); Serial.println(cloud.lastStatus());
            } else {
                Serial.println("No records found — upload some data first");
            }
        }

        fetched = true;
    }

    delay(1000);
}
