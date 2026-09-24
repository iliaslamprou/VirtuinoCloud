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
 * Both store the HTTP status in _last (see lastStatus()).
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
//  Constructor / settings
// ═══════════════════════════════════════════════════════════════════════

VirtuinoCloud::VirtuinoCloud(const char* apiKey) : _key(apiKey) {
    _clientId = nullptr;
    _last     = 0;
    _bPath    = nullptr;
    _bTime[0] = '\0';
    _fCount   = 0;
}

void VirtuinoCloud::setClientId(const char* clientId) {
    _clientId = (clientId && clientId[0]) ? clientId : nullptr;
}

void VirtuinoCloud::_join(char* out, size_t n, const char* path, const char* field) {
    if (path && path[0]) snprintf(out, n, "%s/%s", path, field ? field : "");
    else                 strlcpy(out, field ? field : "", n);
}


// ═══════════════════════════════════════════════════════════════════════
//  read()
//  GET /api/data/field/{full/name}?latest=true
//  Response: { success, field, latest_entry: { time, value, source } | null }
// ═══════════════════════════════════════════════════════════════════════

VirtuinoResult VirtuinoCloud::read(const char* path, const char* field) {
    char name[VC_NAME_LEN];
    _join(name, sizeof(name), path, field);
    return read(name);
}

VirtuinoResult VirtuinoCloud::read(const char* field) {
    VirtuinoResult r;

    char path[VC_NAME_LEN + 40];
    snprintf(path, sizeof(path), "/api/data/field/%s?latest=true", field);

    String resp = _get(path);
    if (!resp.length()) return r;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, resp)) return r;
    if (!doc["success"])            return r;

    JsonObject entry = doc["latest_entry"];
    if (entry.isNull()) return r;   // field exists but has no data yet

    r.ok = true;
    strlcpy(r.value, entry["value"] | "", sizeof(r.value));
    strlcpy(r.time,  entry["time"]  | "", sizeof(r.time));
    return r;
}


// ═══════════════════════════════════════════════════════════════════════
//  readHistory()
//  GET /api/data/field/{full/name}?limit={count}
//  Response: { success, field, count, data: [{time, value, source}, ...] }
//  Returns only the "data" array as a JSON string.
// ═══════════════════════════════════════════════════════════════════════

String VirtuinoCloud::readHistory(const char* path, const char* field, int count) {
    char name[VC_NAME_LEN];
    _join(name, sizeof(name), path, field);
    return readHistory(name, count);
}

String VirtuinoCloud::readHistory(const char* field, int count) {
    char path[VC_NAME_LEN + 40];
    snprintf(path, sizeof(path), "/api/data/field/%s?limit=%d", field, count);

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
//  Body: { api_key, [client_id,] field: "full/name", value [, publish] [, time] }
// ═══════════════════════════════════════════════════════════════════════

bool VirtuinoCloud::write(const char* path, const char* field, float value,
                          bool publish, const char* ts) {
    char name[VC_NAME_LEN];
    _join(name, sizeof(name), path, field);
    return write(name, value, publish, ts);
}

bool VirtuinoCloud::write(const char* field, float value, bool publish, const char* ts) {
    StaticJsonDocument<384> doc;
    doc["api_key"] = _key;
    if (_clientId) doc["client_id"] = _clientId;
    doc["field"]   = field;
    doc["value"]   = value;
    if (publish) doc["publish"] = true;   // omit key entirely when false
    if (ts)      doc["time"]   = ts;      // omit key when not provided

    char body[384];
    serializeJson(doc, body, sizeof(body));
    return _post(body) == 200;
}


// ═══════════════════════════════════════════════════════════════════════
//  Block write — beginWrite / add / send
//  POST /api/data/write
//  Body: { api_key, [client_id,] [path,] [time,] data: [{field, value [, publish]}, ...] }
//  The server joins "path" and each field name with a "/".
// ═══════════════════════════════════════════════════════════════════════

VirtuinoCloud& VirtuinoCloud::beginWrite(const char* path) {
    _bPath    = (path && path[0]) ? path : nullptr;
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
    if (_fCount == 0) return false;

    // Allocate enough space: base object + each field object
    DynamicJsonDocument doc(320 + _fCount * 112);
    doc["api_key"] = _key;
    if (_clientId)  doc["client_id"] = _clientId;
    if (_bPath)     doc["path"]      = _bPath;
    if (_bTime[0])  doc["time"]      = _bTime;   // shared timestamp (optional)

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
    _last = h.POST((uint8_t*)body, strlen(body));
    h.end();
    return _last;
}

String VirtuinoCloud::_get(const char* path) {
    char url[VC_NAME_LEN + 72];
    snprintf(url, sizeof(url), VC_API_BASE "%s", path);
    HTTPClient h;
    h.begin(url);
    h.addHeader("x-api-key", _key);   // API key in header for GET requests
    _last = h.GET();
    String resp = (_last == 200) ? h.getString() : "";
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
    _last = h.POST((uint8_t*)body, strlen(body));
    h.end();
    return _last;
}

String VirtuinoCloud::_get(const char* path) {
    char url[VC_NAME_LEN + 72];
    snprintf(url, sizeof(url), VC_API_BASE "%s", path);
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient h;
    h.begin(client, url);
    h.addHeader("x-api-key", _key);
    _last = h.GET();
    String resp = (_last == 200) ? h.getString() : "";
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
//  The response body is ALWAYS read, even on errors, so the connection is
//  clean for the next request.
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
    _last = _vcHTTP.responseStatusCode();
    _vcHTTP.responseBody();   // drain the reply
    return _last;
}

String VirtuinoCloud::_get(const char* path) {
    _vcHTTP.beginRequest();
    _vcHTTP.get(path);
    _vcHTTP.sendHeader("x-api-key", _key);
    _vcHTTP.endRequest();
    _last = _vcHTTP.responseStatusCode();
    String body = _vcHTTP.responseBody();
    return (_last == 200) ? body : "";
}

#endif // VC_BOARD_WIFININA
