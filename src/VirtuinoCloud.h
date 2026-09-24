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
 *   cloud.write("esp32/temperature", 23.4);
 *
 *   VirtuinoResult r = cloud.read("esp32/relay1");
 *   if (r.ok) digitalWrite(PIN, r.asInt());
 *
 * FIELD NAMES
 * ───────────
 *   A field's name is its full topic, exactly as in Console → Fields —
 *   for example "esp32/temperature". Create every field in the Console
 *   before uploading; the server does not create fields on its own.
 *   Methods that take (path, field) join them with a "/":
 *   cloud.write("esp32", "temperature", 23.4) writes to "esp32/temperature".
 *
 * API KEY: Console → API & Connections
 */

#ifndef VIRTUINO_CLOUD_H
#define VIRTUINO_CLOUD_H

#include <Arduino.h>
#include <ArduinoJson.h>

#define VIRTUINO_CLOUD_VERSION "1.1.0"

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
    // Install ArduinoHttpClient (and WiFiNINA) from Library Manager before compiling.
    // WiFiNINA.h MUST be included here: VirtuinoCloud.cpp is compiled on its own
    // and never sees the sketch's includes. Without it WiFiSSLClient is unknown and
    // the library does not compile on these boards at all (the case up to 1.0.0).
    #include <WiFiNINA.h>
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
#define VC_NAME_LEN    128         // longest full field name ("path/field")
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

    // Optional: a name for this board, e.g. "esp32-kitchen" (1–64 characters
    // from A-Z a-z 0-9 . _ : -). It is sent with every write, so the board
    // appears under this name in My Virtuino World.
    void setClientId(const char* clientId);

    // HTTP status of the last request: 200 = OK, 404 = no field with that
    // name, 403 = read-only API key or wrong key, 429 = too many writes,
    // 0 or negative = no connection. Useful when read() or write() fail.
    int lastStatus() const { return _last; }

    // ── Read ─────────────────────────────────────────────────────────
    //
    // Read the latest stored value of one field. Check .ok before using.
    //
    //   VirtuinoResult r = cloud.read("esp32/temperature");
    //   if (r.ok) Serial.println(r.asFloat());
    //
    //   cloud.read("esp32", "temperature")   // same field, path + name
    VirtuinoResult read(const char* field);
    VirtuinoResult read(const char* path, const char* field);

    // Fetch the last N records of one field as a JSON array String:
    //   [{"time":"...","value":"23.4","source":"HTTP"}, ...]   (newest first)
    // Returns "[]" on network error or if the field has no data.
    // Keep count ≤ 50 on ESP8266 to avoid running out of RAM. Server maximum: 5000.
    //
    //   String h = cloud.readHistory("esp32/temperature", 50);
    String readHistory(const char* field, int count);
    String readHistory(const char* path, const char* field, int count);

    // ── Write single field ────────────────────────────────────────────
    //
    // Upload one field value. Returns true on HTTP 200.
    //
    // publish  — if true, the value is also pushed to the MQTT broker
    //            so live dashboard widgets update immediately (Essential+ plan).
    // ts       — optional ISO 8601 UTC timestamp, e.g. "2024-06-15T14:30:00Z".
    //            If omitted the server records the time of arrival.
    //
    //   cloud.write("esp32/temperature", 23.4);
    //   cloud.write("esp32/temperature", 23.4, true);
    //   cloud.write("esp32/temperature", 23.4, true, "2024-06-15T14:30:00Z");
    //   cloud.write("esp32", "temperature", 23.4);   // same field, path + name
    bool write(const char* field, float value,
               bool publish = false, const char* ts = nullptr);
    bool write(const char* path, const char* field, float value,
               bool publish = false, const char* ts = nullptr);

    // ── Block write ───────────────────────────────────────────────────
    //
    // Queues multiple fields and sends them all in ONE HTTP request.
    //
    //   cloud.beginWrite("esp32");            // optional path, joined to every field
    //   cloud.add("temperature", 23.4);       // → esp32/temperature
    //   cloud.add("humidity",    65.0, true); // publish this field to MQTT
    //   cloud.add("time", "2024-06-15T14:30:00Z");  // optional shared timestamp
    //   bool ok = cloud.send();
    //
    //   cloud.beginWrite();                   // no path: give full names to add()
    //   cloud.add("esp32/temperature", 23.4);
    //
    // send() resets the field list automatically — call beginWrite() again next loop.

    // Start a new block. Clears any previously queued fields.
    VirtuinoCloud& beginWrite(const char* path = nullptr);

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
    const char* _clientId;   // optional, set by setClientId()
    int         _last;       // HTTP status of the last request

    // Block-write state
    const char* _bPath;      // optional path set by beginWrite()
    char        _bTime[32];  // optional timestamp set by add("time",...)
    int         _fCount;     // number of queued fields

    struct _Field {
        char  name[40];    // field name (relative to the path, or full name), max 39
                           // chars — kept small for Uno WiFi Rev2 (6 KB RAM)
        float numVal;      // numeric value  (used when isStr == false)
        char  strVal[32];  // string value   (used when isStr == true)
        bool  isStr;       // which value slot to use when serialising JSON
        bool  publish;     // push to MQTT broker
    } _fields[VC_MAX_FIELDS];

    // "path" + "/" + "field", or just "field" when path is empty.
    static void _join(char* out, size_t n, const char* path, const char* field);

    // Internal HTTP helpers — implemented per board in VirtuinoCloud.cpp.
    // path = "/api/data/field/esp32/temperature?..."  (no host, no scheme)
    int    _post(const char* body);
    String _get(const char* path);
};

#endif // VIRTUINO_CLOUD_H
