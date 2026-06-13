# VirtuinoCloud Arduino Library

Arduino library for the [Virtuino Cloud](https://virtuino.com) IoT platform.
Read and write field data over HTTPS with a simple, unified API.

---

## Supported boards

| Board | WiFi chip | Extra library needed |
|---|---|---|
| ESP32 (all variants) | built-in | — |
| ESP8266 / NodeMCU / Wemos D1 | built-in | — |
| Arduino Uno WiFi Rev2 | NINA-W102 | ArduinoHttpClient |
| Arduino MKR WiFi 1010 | NINA-W102 | ArduinoHttpClient |
| Arduino Nano 33 IoT | NINA-W102 | ArduinoHttpClient |
| Arduino Nano RP2040 Connect | NINA-W102 | ArduinoHttpClient |

> **ESP8266 note:** requires board core ≥ 2.5.0.  
> IDE → File → Preferences → Additional Boards Manager URLs:  
> `https://arduino.esp8266.com/stable/package_esp8266com_index.json`  
> Then: Tools → Boards Manager → **esp8266 by ESP8266 Community** → install / update to 3.x

---

## Installation

### Option A — Library Manager (recommended)
1. Arduino IDE → Sketch → Include Library → Manage Libraries
2. Search for **VirtuinoCloud** → Install
3. Also install **ArduinoJson** (by Benoit Blanchon)
4. WiFiNINA boards only: also install **ArduinoHttpClient**

### Option B — ZIP
1. Download `VirtuinoCloud.zip` from [virtuino.com/downloads](https://virtuino.com)
2. IDE → Sketch → Include Library → Add .ZIP Library

---

## Quick start

```cpp
#include <WiFi.h>            // <ESP8266WiFi.h> on ESP8266, <WiFiNINA.h> on MKR/Nano
#include <VirtuinoCloud.h>

VirtuinoCloud cloud("YOUR_API_KEY");

void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop() {
    // Write one field
    cloud.write("my-device", "temperature", 23.4);

    // Read one field
    VirtuinoResult r = cloud.read("my-device", "relay1");
    if (r.ok) digitalWrite(LED_PIN, r.asInt());

    delay(10000);
}
```

---

## API Reference

### Read latest value

```cpp
VirtuinoResult r = cloud.read("device", "field");

r.ok           // bool   — false on network or parse error
r.asFloat()    // float  — e.g. 23.4
r.asInt()      // int    — e.g. 23
r.asString()   // String — e.g. "23.4"
r.time         // char[] — ISO 8601 timestamp e.g. "2024-06-15T14:30:00Z"
r.asJson()     // String — {"value":"23.4","time":"2024-06-15T14:30:00Z"}
```

### Read history

```cpp
// Returns a JSON array String with the last N records
String h = cloud.readHistory("device", "field", 50);
// → [{"time":"2024-06-15T14:30:00Z","value":"23.4"}, ...]

// Parse with ArduinoJson if needed:
DynamicJsonDocument doc(8192);
deserializeJson(doc, h);
for (JsonObject rec : doc.as<JsonArray>()) {
    Serial.println(rec["value"].as<const char*>());
}
```

> Max count = 5000. Keep ≤ 50 on ESP8266 (limited RAM).

### Write single field

```cpp
cloud.write("device", "field", 23.4);                         // basic
cloud.write("device", "field", 23.4, true);                   // + publish to MQTT
cloud.write("device", "field", 23.4, true, "2024-06-15T14:30:00Z"); // + timestamp
```

Returns `true` if the server responded with HTTP 200.

> `publish: true` pushes the value to the MQTT broker in real time (Essential+ plan).

### Block write — multiple fields in one HTTP request

```cpp
cloud.beginWrite("device");
cloud.add("temperature", 23.4);
cloud.add("humidity",    65.0, true);   // publish this field
cloud.add("pressure",    1013.0);
cloud.add("time", "2024-06-15T14:30:00Z");  // optional shared timestamp
bool ok = cloud.send();                 // sends ONE HTTP request for all fields
```

`send()` resets the field list automatically so `beginWrite` can be called again next loop.

---

## Examples

| Example | Description |
|---|---|
| `01_WriteField` | Upload one sensor value |
| `02_BlockWrite` | Upload multiple fields in one request |
| `03_ReadLatest` | Read a field and control a GPIO |
| `04_ReadHistory` | Fetch last 50 records and calculate average |
| `05_Thermostat` | Read setpoint from dashboard + bang-bang heater control |

---

## Console setup

Before uploading any sketch:
1. Log in at [virtuino.com](https://virtuino.com)
2. **Console → Devices** — create a device and add its fields
3. **Console → API & Connections** — copy your API key

The `device` string in your sketch must match the device name in the Console exactly.
