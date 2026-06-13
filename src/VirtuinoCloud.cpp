/*
 * VirtuinoCloud.cpp
 * =================
 * Implementation of the VirtuinoCloud class.
 *
 * The file is compiled once for the target board. The correct HTTP backend
 * is selected by the #ifdef blocks at the bottom:
 *
 *   VC_BOARD_ESP32    → HTTPClient (built into ESP32 Arduino core)
 *   VC_BOARD_ESP8266  → ESP8266HTTPClient + WiFiClientSecure (setInsecure)
 *   VC_BOARD_WIFININA → ArduinoHttpClient + WiFiSSLClient
 *
 * All three backends expose the same two private methods:
 *   int    _post(body)  — POST JSON body, returns HTTP status code
 *   String _get(path)   — GET with x-api-key header, returns response body
 */

#include "VirtuinoCloud.h"

// ── WiFiNINA: persistent objects required by ArduinoHttpClient ────────────────
// HttpClient constructor takes (client, host, port) — must be constructed at
// file scope so they survive between calls.
#ifdef VC_BOARD_WIFININA
static WiFiSSLClient _vcSSL;
static HttpClient    _vcHTTP(_vcSSL, VC_API_HOST, 443);
#endif


// ═══════════════════════════════════════════════════════════════════════
//  VirtuinoResult
// ═══════════════════════════════════════════════════════════════════════

String VirtuinoResult::asJson() {
    // Builds: {"value":"23.4","time":"2024-06-15T14:30:00Z"}
    String j = "{\"value\":\""; j += value;
    j += "\",\"time\":\"";      j += time;
    j += "\"}";
    return j;
}


// ═══════════════════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════════════════

VirtuinoCloud::VirtuinoCloud(const char* apiKey) : _key(apiKey) {
    _bDevice  = nullptr;
    _bTime[0] = '\0';
    _fCount   = 0;
}


// ═══════════════════════════════════════════════════════════════════════
//  read()
//  GET /api/data/device/{device}/field/{field}?latest=true
//  Response: { success, latest_entry: { value, time, device, source } }
// ═══════════════════════════════════════════════════════════════════════

VirtuinoResult VirtuinoCloud::read(const char* device, const char* field) {
    VirtuinoResult r;

    char path[160];
    snprintf(path, sizeof(path),
        "/api/data/device/%s/field/%s?latest=true", device, field);

    String resp = _get(path);
    if (!resp.length()) { Serial.println("[VC] read: no response"); return r; }
    Serial.print("[VC] read response: "); Serial.println(resp);

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, resp)) { Serial.println("[VC] read: JSON parse error"); return r; }
    if (!doc["success"])            { Serial.print("[VC] read: success=false, error="); Serial.println(doc["error"] | ""); return r; }

    JsonObject entry = doc["latest_entry"];
    if (entry.isNull()) return r;   // field exists but has no data yet

    r.ok = true;
    strlcpy(r.value, entry["value"] | "", sizeof(r.value));
    strlcpy(r.time,  entry["time"]  | "", sizeof(r.time));
    return r;
}


// ═══════════════════════════════════════════════════════════════════════
//  readHistory()
//  GET /api/data/device/{device}/field/{field}?limit={count}
//  Response: { success, count, data: [{time, value, device, source}, ...] }
//  Returns only the "data" array as a JSON string.
// ═══════════════════════════════════════════════════════════════════════

String VirtuinoCloud::readHistory(const char* device, const char* field, int count) {
    char path[160];
    snprintf(path, sizeof(path),
        "/api/data/device/%s/field/%s?limit=%d", device, field, count);

    String resp = _get(path);
    if (!resp.length()) return "[]";

    // Heap allocation — history responses can be several KB
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, resp)) return "[]";
    if (!doc["success"])            return "[]";

    // Serialize only the "data" array so the caller gets a clean JSON array
    String out;
    serializeJson(doc["data"], out);
    return out;
}


// ═══════════════════════════════════════════════════════════════════════
//  write()
//  POST /api/data/write
//  Body: { api_key, device_name, field, value [, publish] [, time] }
// ═══════════════════════════════════════════════════════════════════════

bool VirtuinoCloud::write(const char* device, const char* field, float value,
                          bool publish, const char* ts) {
    StaticJsonDocument<256> doc;
    doc["api_key"]     = _key;
    doc["device_name"] = device;
    doc["field"]       = field;
    doc["value"]       = value;
    if (publish) doc["publish"] = true;   // omit key entirely when false
    if (ts)      doc["time"]   = ts;      // omit key when not provided

    char body[256];
    serializeJson(doc, body, sizeof(body));
    return _post(body) == 200;
}


// ═══════════════════════════════════════════════════════════════════════
//  Block write — beginWrite / add / send
//  POST /api/data/write
//  Body: { api_key, device_name [, time], data: [{field, value [, publish]}, ...] }
// ═══════════════════════════════════════════════════════════════════════

VirtuinoCloud& VirtuinoCloud::beginWrite(const char* device) {
    _bDevice  = device;
    _fCount   = 0;
    _bTime[0] = '\0';
    return *this;
}

VirtuinoCloud& VirtuinoCloud::add(const char* field, float value, bool publish) {
    if (_fCount >= VC_MAX_FIELDS) return *this;   // silently drop if full
    strlcpy(_fields[_fCount].name, field, sizeof(_fields[0].name));
    _fields[_fCount].numVal  = value;
    _fields[_fCount].isStr   = false;
    _fields[_fCount].publish = publish;
    _fCount++;
    return *this;
}

VirtuinoCloud& VirtuinoCloud::add(const char* field, const char* value) {
    // "time" is a reserved key — it goes to the top-level body, not the data array
    if (strcmp(field, "time") == 0) {
        strlcpy(_bTime, value, sizeof(_bTime));
        return *this;
    }
    if (_fCount >= VC_MAX_FIELDS) return *this;
    strlcpy(_fields[_fCount].name,   field, sizeof(_fields[0].name));
    strlcpy(_fields[_fCount].strVal, value, sizeof(_fields[0].strVal));
    _fields[_fCount].isStr   = true;
    _fields[_fCount].publish = false;
    _fCount++;
    return *this;
}

bool VirtuinoCloud::send() {
    if (!_bDevice || _fCount == 0) return false;

    // Allocate enough space: base object + each field object
    DynamicJsonDocument doc(256 + _fCount * 96);
    doc["api_key"]     = _key;
    doc["device_name"] = _bDevice;
    if (_bTime[0]) doc["time"] = _bTime;   // shared timestamp (optional)

    JsonArray arr = doc.createNestedArray("data");
    for (int i = 0; i < _fCount; i++) {
        JsonObject o = arr.createNestedObject();
        o["field"] = _fields[i].name;
        // Preserve the original type (float vs string) in the JSON output
        if (_fields[i].isStr) o["value"] = _fields[i].strVal;
        else                  o["value"] = _fields[i].numVal;
        if (_fields[i].publish) o["publish"] = true;
    }

    String body;
    serializeJson(doc, body);

    // Reset state so beginWrite() can be called again next loop iteration
    _fCount   = 0;
    _bTime[0] = '\0';

    return _post(body.c_str()) == 200;
}


// ═══════════════════════════════════════════════════════════════════════
//  HTTP backend — ESP32
//  Uses the HTTPClient bundled with the ESP32 Arduino core.
//  TLS is handled transparently by the underlying mbedTLS stack.
// ═══════════════════════════════════════════════════════════════════════
#ifdef VC_BOARD_ESP32

int VirtuinoCloud::_post(const char* body) {
    HTTPClient h;
    h.begin(VC_API_BASE "/api/data/write");
    h.addHeader("Content-Type", "application/json");
    int code = h.POST((uint8_t*)body, strlen(body));
    h.end();
    return code;
}

String VirtuinoCloud::_get(const char* path) {
    char url[192];
    snprintf(url, sizeof(url), VC_API_BASE "%s", path);
    HTTPClient h;
    h.begin(url);
    h.addHeader("x-api-key", _key);   // API key in header for GET requests
    int code = h.GET();
    String resp = (code == 200) ? h.getString() : "";
    h.end();
    return resp;
}

#endif // VC_BOARD_ESP32


// ═══════════════════════════════════════════════════════════════════════
//  HTTP backend — ESP8266
//  Uses ESP8266HTTPClient + WiFiClientSecure.
//  setInsecure() skips certificate verification — acceptable for IoT
//  data uploads on a trusted network. Requires core >= 2.5.0.
// ═══════════════════════════════════════════════════════════════════════
#ifdef VC_BOARD_ESP8266

int VirtuinoCloud::_post(const char* body) {
    WiFiClientSecure client;
    client.setInsecure();   // skip TLS certificate check
    HTTPClient h;
    h.begin(client, VC_API_BASE "/api/data/write");
    h.addHeader("Content-Type", "application/json");
    int code = h.POST((uint8_t*)body, strlen(body));
    h.end();
    return code;
}

String VirtuinoCloud::_get(const char* path) {
    char url[192];
    snprintf(url, sizeof(url), VC_API_BASE "%s", path);
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient h;
    h.begin(client, url);
    h.addHeader("x-api-key", _key);
    int code = h.GET();
    String resp = (code == 200) ? h.getString() : "";
    h.end();
    return resp;
}

#endif // VC_BOARD_ESP8266


// ═══════════════════════════════════════════════════════════════════════
//  HTTP backend — WiFiNINA
//  Uses ArduinoHttpClient with WiFiSSLClient for HTTPS.
//  The static _vcSSL and _vcHTTP objects are defined at the top of
//  this file — ArduinoHttpClient requires them to persist between calls.
//
//  POST pattern: beginRequest → post → sendHeader(s) → endRequest → print(body)
//  GET  pattern: beginRequest → get  → sendHeader(s) → endRequest → responseBody()
// ═══════════════════════════════════════════════════════════════════════
#ifdef VC_BOARD_WIFININA

int VirtuinoCloud::_post(const char* body) {
    int len = strlen(body);
    _vcHTTP.beginRequest();
    _vcHTTP.post("/api/data/write");
    _vcHTTP.sendHeader("Content-Type",   "application/json");
    _vcHTTP.sendHeader("Content-Length", len);
    _vcHTTP.endRequest();
    _vcHTTP.print(body);   // body is sent after the headers
    return _vcHTTP.responseStatusCode();
}

String VirtuinoCloud::_get(const char* path) {
    _vcHTTP.beginRequest();
    _vcHTTP.get(path);
    _vcHTTP.sendHeader("x-api-key", _key);
    _vcHTTP.endRequest();
    int code = _vcHTTP.responseStatusCode();
    return (code == 200) ? _vcHTTP.responseBody() : "";
}

#endif // VC_BOARD_WIFININA
