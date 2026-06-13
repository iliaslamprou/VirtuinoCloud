/*
 * VirtuinoCloud.h — Arduino library for Virtuino Cloud
 * ======================================================
 * Read and write IoT field data to Virtuino Cloud over HTTPS.
 *
 * SUPPORTED BOARDS
 * ─────────────────
 *   ESP32 (all variants)
 *   ESP8266 / NodeMCU / Wemos D1 Mini  (core ≥ 2.5.0 required)
 *   Arduino Uno WiFi Rev2
 *   Arduino MKR WiFi 1010
 *   Arduino Nano 33 IoT
 *   Arduino Nano RP2040 Connect
 *
 * REQUIRED LIBRARIES  (Sketch → Include Library → Manage Libraries)
 * ──────────────────
 *   ArduinoJson          — by Benoit Blanchon  (all boards)
 *   ArduinoHttpClient    — by Arduino           (WiFiNINA boards only)
 *
 * QUICK START
 * ───────────
 *   #include <WiFi.h>          // or <ESP8266WiFi.h> / <WiFiNINA.h>
 *   #include <VirtuinoCloud.h>
 *
 *   VirtuinoCloud cloud("YOUR_API_KEY");
 *
 *   cloud.write("my-device", "temperature", 23.4);
 *
 *   VirtuinoResult r = cloud.read("my-device", "relay1");
 *   if (r.ok) digitalWrite(PIN, r.asInt());
 *
 * API KEY: Console → API & Connections
 * DEVICES: Console → Devices  (create device + fields before uploading)
 */

#ifndef VIRTUINO_CLOUD_H
#define VIRTUINO_CLOUD_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════════════════
//  Board detection — selects the correct HTTP backend automatically.
//  You do NOT need to change anything here.
// ═══════════════════════════════════════════════════════════════════════

#if defined(ESP32)
    // ESP32: uses the built-in HTTPClient over plain WiFiClient (TLS handled by IDF)
    #include <HTTPClient.h>
    #define VC_BOARD_ESP32

#elif defined(ESP8266)
    // ESP8266: uses HTTPClient + WiFiClientSecure with setInsecure()
    // Requires board core >= 2.5.0.
    // To update: Arduino IDE → Tools → Boards Manager → "esp8266 by ESP8266 Community" → 3.x
    // Boards Manager URL (add in File → Preferences):
    //   https://arduino.esp8266.com/stable/package_esp8266com_index.json
    #ifndef ARDUINO_ESP8266_MAJOR
        #error "VirtuinoCloud requires ESP8266 core >= 2.5.0. Go to: Tools → Boards Manager → esp8266 by ESP8266 Community → update to version 3.x"
    #endif
    #include <ESP8266HTTPClient.h>
    #include <WiFiClientSecure.h>
    #define VC_BOARD_ESP8266

#elif defined(ARDUINO_AVR_UNO_WIFI_REV2)  || \
      defined(ARDUINO_SAMD_MKRWIFI1010)   || \
      defined(ARDUINO_SAMD_NANO_33_IOT)   || \
      defined(ARDUINO_NANO_RP2040_CONNECT)
    // WiFiNINA boards: uses ArduinoHttpClient + WiFiSSLClient
    // Install ArduinoHttpClient from Library Manager before compiling.
    #include <ArduinoHttpClient.h>
    #define VC_BOARD_WIFININA

#else
    #error "VirtuinoCloud: board not supported. Supported boards: ESP32, ESP8266, "\
           "Uno WiFi Rev2, MKR WiFi 1010, Nano 33 IoT, Nano RP2040 Connect."
#endif

// ═══════════════════════════════════════════════════════════════════════
//  Configuration
// ═══════════════════════════════════════════════════════════════════════

#define VC_MAX_FIELDS  16          // maximum fields per beginWrite / send block
#define VC_API_HOST    "api.virtuino.com"
#define VC_API_BASE    "https://" VC_API_HOST


// ═══════════════════════════════════════════════════════════════════════
//  VirtuinoResult
//  Returned by cloud.read(). Always check .ok before using the value.
// ═══════════════════════════════════════════════════════════════════════

struct VirtuinoResult {
    bool ok        = false;   // true only when the server returned a valid value
    char value[32] = "";      // raw value string,    e.g. "23.4"
    char time[32]  = "";      // ISO 8601 timestamp,  e.g. "2024-06-15T14:30:00Z"

    float  asFloat()  { return atof(value); }   // e.g. 23.4
    int    asInt()    { return atoi(value); }   // e.g. 23
    String asString() { return String(value); } // e.g. "23.4"

    // Compact JSON combining value + timestamp.
    // Example: {"value":"23.4","time":"2024-06-15T14:30:00Z"}
    String asJson();
};


// ═══════════════════════════════════════════════════════════════════════
//  VirtuinoCloud
// ═══════════════════════════════════════════════════════════════════════

class VirtuinoCloud {
public:
    // apiKey — copy from Console → API & Connections
    explicit VirtuinoCloud(const char* apiKey);

    // ── Read ─────────────────────────────────────────────────────────

    // Read the latest stored value of one field.
    // Returns a VirtuinoResult; check .ok before using.
    //
    // Example:
    //   VirtuinoResult r = cloud.read("my-device", "temperature");
    //   if (r.ok) Serial.println(r.asFloat());
    VirtuinoResult read(const char* device, const char* field);

    // Fetch the last N records of one field as a JSON array String.
    // Returns: [{"time":"...","value":"23.4"}, {"time":"...","value":"23.1"}, ...]
    // Returns "[]" on network error or if the field has no data.
    //
    // Parse the result with ArduinoJson if you need to process individual records.
    // Keep count ≤ 50 on ESP8266 to avoid running out of RAM.
    // Maximum count accepted by the server: 5000.
    //
    // Example:
    //   String h = cloud.readHistory("my-device", "temperature", 50);
    //   DynamicJsonDocument doc(8192);
    //   deserializeJson(doc, h);
    //   for (JsonObject rec : doc.as<JsonArray>()) { ... }
    String readHistory(const char* device, const char* field, int count);

    // ── Write single field ────────────────────────────────────────────

    // Upload one field value. Returns true on HTTP 200.
    //
    // publish  — if true, the value is also pushed to the MQTT broker
    //            so live dashboard widgets update immediately (Essential+ plan).
    // ts       — optional ISO 8601 UTC timestamp, e.g. "2024-06-15T14:30:00Z".
    //            If omitted the server records the time of arrival.
    //
    // Examples:
    //   cloud.write("my-device", "temperature", 23.4);
    //   cloud.write("my-device", "temperature", 23.4, true);
    //   cloud.write("my-device", "temperature", 23.4, true, "2024-06-15T14:30:00Z");
    bool write(const char* device, const char* field, float value,
               bool publish = false, const char* ts = nullptr);

    // ── Block write ───────────────────────────────────────────────────
    //
    // Queues multiple fields and sends them all in ONE HTTP request.
    // This avoids making N separate requests when you have N fields to upload.
    //
    // Usage:
    //   cloud.beginWrite("my-device");
    //   cloud.add("temperature", 23.4);
    //   cloud.add("humidity",    65.0, true);  // publish this field to MQTT
    //   cloud.add("time", "2024-06-15T14:30:00Z");  // optional shared timestamp
    //   bool ok = cloud.send();
    //
    // send() resets the field list automatically — call beginWrite() again next loop.

    // Start a new block for the given device. Clears any previously queued fields.
    VirtuinoCloud& beginWrite(const char* device);

    // Add a numeric field to the current block.
    // publish=true → also push this field to the MQTT broker (Essential+ plan).
    VirtuinoCloud& add(const char* field, float value, bool publish = false);

    // Add a string field to the current block.
    // Special case: add("time", "2024-06-15T14:30:00Z") sets a shared timestamp
    // for all fields in this block instead of adding a data field.
    VirtuinoCloud& add(const char* field, const char* value);

    // Send all queued fields in one HTTP POST. Returns true on HTTP 200.
    // Resets the field list so beginWrite() can be called again next loop.
    bool send();

private:
    const char* _key;

    // Block-write state
    const char* _bDevice;    // device set by beginWrite()
    char        _bTime[32];  // optional timestamp set by add("time",...)
    int         _fCount;     // number of queued fields

    struct _Field {
        char  name[32];    // field name
        float numVal;      // numeric value  (used when isStr == false)
        char  strVal[32];  // string value   (used when isStr == true)
        bool  isStr;       // which value slot to use when serialising JSON
        bool  publish;     // push to MQTT broker
    } _fields[VC_MAX_FIELDS];

    // Internal HTTP helpers — implemented per board in VirtuinoCloud.cpp.
    // path = "/api/data/device/X/field/Y?..."  (no host, no scheme)
    int    _post(const char* body);
    String _get(const char* path);
};

#endif // VIRTUINO_CLOUD_H
