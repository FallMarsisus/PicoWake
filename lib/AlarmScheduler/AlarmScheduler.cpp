#include "AlarmScheduler.h"
#include "AppPage.h"
#include <ArduinoJson.h>
#include <LEAmDNS.h>

#define ALARM_LOG(fmt, ...) Serial.printf("[ALARM] " fmt "\n", ##__VA_ARGS__)

static constexpr uint8_t kAlarmBuzzerPin = 14;
static inline void alarm_buzzer_on() { digitalWrite(kAlarmBuzzerPin, HIGH); }
static inline void alarm_buzzer_off() { digitalWrite(kAlarmBuzzerPin, LOW); }

void AlarmScheduler::begin(const char* storagePath) {
    storagePath_ = storagePath ? storagePath : "/alarms.json";
    loadFromDisk();
    setupRoutes();
}

void AlarmScheduler::ensureWebStarted(const char* mdnsHost) {
    if (webRunning_) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    server_.begin();
    webRunning_ = true;

    const char* host = (mdnsHost && strlen(mdnsHost) > 0) ? mdnsHost : "picowake";
    const bool ok = MDNS.begin(host);
    ALARM_LOG("web started ip=%s mdns=%s.local mdnsOk=%d", WiFi.localIP().toString().c_str(), host, ok ? 1 : 0);
}

void AlarmScheduler::stopWeb() {
    if (!webRunning_) {
        return;
    }

    server_.stop();
    MDNS.end();
    webRunning_ = false;
    ALARM_LOG("web stopped");
}

bool AlarmScheduler::isWebRunning() const {
    return webRunning_;
}

bool AlarmScheduler::isRinging() const {
    return ringing_;
}

void AlarmScheduler::dismissRinging() {
    if (!ringing_) {
        return;
    }
    ALARM_LOG("dismiss requested by touch");
    stopRing();
}

void AlarmScheduler::snoozeRinging(uint32_t snoozeMinutes) {
    if (!ringing_) {
        return;
    }

    if (snoozeMinutes == 0) {
        snoozeMinutes = 10;
    }

    const String alarmLabel = ringingLabel_;
    stopRing();
    startSnooze(snoozeMinutes, alarmLabel);
    ALARM_LOG("snooze requested for %lu min label=%s", static_cast<unsigned long>(snoozeMinutes), alarmLabel.c_str());
}

size_t AlarmScheduler::activeEnabledCount() const {
    size_t count = 0;
    for (size_t i = 0; i < alarmCount_; ++i) {
        if (alarms_[i].enabled) {
            ++count;
        }
    }
    return count;
}

String AlarmScheduler::currentRingingLabel() const {
    return ringingLabel_;
}

bool AlarmScheduler::hasSnooze() const {
    return snoozeActive_;
}

uint32_t AlarmScheduler::snoozeRemainingSeconds() const {
    if (!snoozeActive_) {
        return 0;
    }

    const uint32_t now = millis();
    if (now >= snoozeDueMs_) {
        return 0;
    }

    return (snoozeDueMs_ - now + 999U) / 1000U;
}

uint16_t AlarmScheduler::nightStartMinuteForDay(uint8_t dayIndex) const {
    return nightStartMinuteByDay_[dayIndex % 7];
}

uint16_t AlarmScheduler::nightEndMinuteForDay(uint8_t dayIndex) const {
    return nightEndMinuteByDay_[dayIndex % 7];
}

uint32_t AlarmScheduler::settingsVersion() const {
    return settingsVersion_;
}

void AlarmScheduler::loop() {
    if (webRunning_) {
        server_.handleClient();
        MDNS.update();
    }

    if (snoozeActive_ && millis() >= snoozeDueMs_) {
        snoozeActive_ = false;
        startRing(ringingVolume_, snoozeLabel_);
        ALARM_LOG("snooze fired label=%s", snoozeLabel_.c_str());
    }

    evaluateAlarms();
    updateRing();
}

void AlarmScheduler::handleGetManifest() {
    // Ce JSON est requis par Android (Chrome) pour afficher "Ajouter à l'écran d'accueil"
    String json = R"JSON({
  "name": "PicoWake",
  "short_name": "PicoWake",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#0b0f16",
  "theme_color": "#0b0f16",
  "icons": [
    {
      "src": "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' fill='%230b0f16'/><circle cx='50' cy='50' r='30' fill='%232dd4bf'/></svg>",
      "sizes": "192x192",
      "type": "image/svg+xml"
    },
    {
      "src": "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' fill='%230b0f16'/><circle cx='50' cy='50' r='30' fill='%232dd4bf'/></svg>",
      "sizes": "512x512",
      "type": "image/svg+xml"
    }
  ]
})JSON";
    server_.send(200, "application/manifest+json", json);
}



String AlarmScheduler::buildAppPage() const {
    return String(kAppPageHtml); 
}

void AlarmScheduler::handleGetRoot() {
    server_.send(200, "text/html; charset=utf-8", buildAppPage());
}

void AlarmScheduler::handleGetState() {
    DynamicJsonDocument doc(8192);
    doc["ok"] = true;
    doc["volume"] = masterVolume_;
    doc["settingsVersion"] = settingsVersion_;
    JsonArray nightArr = doc.createNestedArray("nightWindows");
    for (uint8_t day = 0; day < 7; ++day) {
        JsonObject w = nightArr.createNestedObject();
        w["day"] = day;
        w["startMinute"] = nightStartMinuteByDay_[day];
        w["endMinute"] = nightEndMinuteByDay_[day];
    }
    JsonArray arr = doc.createNestedArray("alarms");

    for (size_t i = 0; i < alarmCount_; ++i) {
        JsonObject a = arr.createNestedObject();
        a["id"] = alarms_[i].id;
        a["hour"] = alarms_[i].hour;
        a["minute"] = alarms_[i].minute;
        a["daysMask"] = alarms_[i].daysMask;
        a["enabled"] = alarms_[i].enabled;
        a["volume"] = alarms_[i].volume;
        a["label"] = alarms_[i].label;
    }

    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void AlarmScheduler::handleCreateAlarm() {
    DynamicJsonDocument in(1024);
    if (deserializeJson(in, server_.arg("plain")) != DeserializationError::Ok) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    if (alarmCount_ >= kMaxAlarms) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"max alarms reached\"}");
        return;
    }

    Alarm a;
    a.id = nextId_++;
    a.hour = clampToByte(in["hour"] | 7, 0, 23);
    a.minute = clampToByte(in["minute"] | 0, 0, 59);
    a.daysMask = clampToByte(in["daysMask"] | 0x7F, 0, 0x7F);
    a.enabled = in["enabled"].isNull() ? true : static_cast<bool>(in["enabled"]);
    a.volume = clampToByte(in["volume"] | masterVolume_, 0, 100);
    a.label = String((const char*)(in["label"] | ""));

    alarms_[alarmCount_++] = a;
    saveToDisk();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::handleUpdateAlarm() {
    DynamicJsonDocument in(1024);
    if (deserializeJson(in, server_.arg("plain")) != DeserializationError::Ok) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    const int id = in["id"] | -1;
    const int idx = findAlarmIndexById(static_cast<uint16_t>(id));
    if (idx < 0) {
        server_.send(404, "application/json", "{\"ok\":false,\"error\":\"alarm not found\"}");
        return;
    }

    Alarm& a = alarms_[idx];
    if (!in["hour"].isNull()) a.hour = clampToByte(in["hour"], 0, 23);
    if (!in["minute"].isNull()) a.minute = clampToByte(in["minute"], 0, 59);
    if (!in["daysMask"].isNull()) a.daysMask = clampToByte(in["daysMask"], 0, 0x7F);
    if (!in["enabled"].isNull()) a.enabled = static_cast<bool>(in["enabled"]);
    if (!in["volume"].isNull()) a.volume = clampToByte(in["volume"], 0, 100);
    if (!in["label"].isNull()) a.label = String((const char*)in["label"]);

    saveToDisk();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::handleDeleteAlarm() {
    const int id = server_.arg("id").toInt();
    const int idx = findAlarmIndexById(static_cast<uint16_t>(id));
    if (idx < 0) {
        server_.send(404, "application/json", "{\"ok\":false,\"error\":\"alarm not found\"}");
        return;
    }

    for (size_t i = static_cast<size_t>(idx); i + 1 < alarmCount_; ++i) {
        alarms_[i] = alarms_[i + 1];
    }
    if (alarmCount_ > 0) {
        --alarmCount_;
    }

    saveToDisk();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::handleSetVolume() {
    DynamicJsonDocument in(256);
    if (deserializeJson(in, server_.arg("plain")) != DeserializationError::Ok) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    masterVolume_ = clampToByte(in["value"] | 70, 0, 100);
    saveToDisk();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::handleSetEnabled() {
    DynamicJsonDocument in(256);
    if (deserializeJson(in, server_.arg("plain")) != DeserializationError::Ok) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    const int id = in["id"] | -1;
    const int idx = findAlarmIndexById(static_cast<uint16_t>(id));
    if (idx < 0) {
        server_.send(404, "application/json", "{\"ok\":false,\"error\":\"alarm not found\"}");
        return;
    }

    alarms_[idx].enabled = static_cast<bool>(in["enabled"] | false);
    saveToDisk();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::handleSetNightWindow() {
    DynamicJsonDocument in(2048);
    if (deserializeJson(in, server_.arg("plain")) != DeserializationError::Ok) {
        server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    bool changed = false;
    JsonArray windows = in["windows"].as<JsonArray>();
    if (!windows.isNull()) {
        bool seenDay[7] = {false, false, false, false, false, false, false};
        for (JsonObject w : windows) {
            const int day = w["day"] | -1;
            const int startMinute = w["startMinute"] | -1;
            const int endMinute = w["endMinute"] | -1;
            if (day < 0 || day > 6 || startMinute < 0 || startMinute > 1439 || endMinute < 0 || endMinute > 1439) {
                server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid range\"}");
                return;
            }
            seenDay[day] = true;

            const uint16_t s = static_cast<uint16_t>(startMinute);
            const uint16_t e = static_cast<uint16_t>(endMinute);
            if (nightStartMinuteByDay_[day] != s || nightEndMinuteByDay_[day] != e) {
                nightStartMinuteByDay_[day] = s;
                nightEndMinuteByDay_[day] = e;
                changed = true;
            }
        }

        for (uint8_t day = 0; day < 7; ++day) {
            if (!seenDay[day]) {
                server_.send(400, "application/json", "{\"ok\":false,\"error\":\"all days required\"}");
                return;
            }
        }
    } else {
        // Backward compatibility with old payload format.
        const int startMinute = in["startMinute"] | -1;
        const int endMinute = in["endMinute"] | -1;
        if (startMinute < 0 || startMinute > 1439 || endMinute < 0 || endMinute > 1439) {
            server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid range\"}");
            return;
        }

        const uint16_t s = static_cast<uint16_t>(startMinute);
        const uint16_t e = static_cast<uint16_t>(endMinute);
        for (uint8_t day = 0; day < 7; ++day) {
            if (nightStartMinuteByDay_[day] != s || nightEndMinuteByDay_[day] != e) {
                nightStartMinuteByDay_[day] = s;
                nightEndMinuteByDay_[day] = e;
                changed = true;
            }
        }
    }

    if (changed) {
        ++settingsVersion_;
        saveToDisk();
    }
    server_.send(200, "application/json", "{\"ok\":true}");
}

void AlarmScheduler::loadFromDisk() {
    alarmCount_ = 0;
    nextId_ = 1;
    masterVolume_ = 70;
    settingsVersion_ = 1;
    for (uint8_t day = 0; day < 7; ++day) {
        nightStartMinuteByDay_[day] = 21 * 60;
        nightEndMinuteByDay_[day] = 7 * 60;
    }

    if (!LittleFS.exists(storagePath_)) {
        ALARM_LOG("no alarm file, starting fresh");
        return;
    }

    File f = LittleFS.open(storagePath_, "r");
    if (!f) {
        ALARM_LOG("failed to open %s", storagePath_.c_str());
        return;
    }

    DynamicJsonDocument doc(6144);
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        ALARM_LOG("json parse failed: %s", err.c_str());
        return;
    }

    masterVolume_ = clampToByte(doc["masterVolume"] | 70, 0, 100);
    settingsVersion_ = static_cast<uint32_t>(doc["settingsVersion"] | 1);

    JsonArray nightArr = doc["nightWindows"].as<JsonArray>();
    if (!nightArr.isNull()) {
        for (JsonObject w : nightArr) {
            const int day = w["day"] | -1;
            if (day < 0 || day > 6) {
                continue;
            }
            nightStartMinuteByDay_[day] = static_cast<uint16_t>(constrain(w["startMinute"] | 1260, 0, 1439));
            nightEndMinuteByDay_[day] = static_cast<uint16_t>(constrain(w["endMinute"] | 420, 0, 1439));
        }
    } else {
        // Backward compatibility with previous global night window values.
        const uint16_t globalStart = static_cast<uint16_t>(constrain(doc["nightStartMinute"] | 1260, 0, 1439));
        const uint16_t globalEnd = static_cast<uint16_t>(constrain(doc["nightEndMinute"] | 420, 0, 1439));
        for (uint8_t day = 0; day < 7; ++day) {
            nightStartMinuteByDay_[day] = globalStart;
            nightEndMinuteByDay_[day] = globalEnd;
        }
    }

    JsonArray arr = doc["alarms"].as<JsonArray>();
    for (JsonObject it : arr) {
        if (alarmCount_ >= kMaxAlarms) break;
        Alarm& a = alarms_[alarmCount_++];
        a.id = static_cast<uint16_t>(it["id"] | nextId_++);
        a.hour = clampToByte(it["hour"] | 7, 0, 23);
        a.minute = clampToByte(it["minute"] | 0, 0, 59);
        a.daysMask = clampToByte(it["daysMask"] | 0x7F, 0, 0x7F);
        a.enabled = it["enabled"].isNull() ? true : static_cast<bool>(it["enabled"]);
        a.volume = clampToByte(it["volume"] | masterVolume_, 0, 100);
        a.label = String((const char*)(it["label"] | ""));
        if (a.id >= nextId_) nextId_ = a.id + 1;
    }

    ALARM_LOG("loaded alarms=%u volume=%u nightDimanche=%02u:%02u-%02u:%02u",
              static_cast<unsigned>(alarmCount_),
              static_cast<unsigned>(masterVolume_),
              static_cast<unsigned>(nightStartMinuteByDay_[0] / 60),
              static_cast<unsigned>(nightStartMinuteByDay_[0] % 60),
              static_cast<unsigned>(nightEndMinuteByDay_[0] / 60),
              static_cast<unsigned>(nightEndMinuteByDay_[0] % 60));
}

void AlarmScheduler::saveToDisk() {
    DynamicJsonDocument doc(6144);
    doc["masterVolume"] = masterVolume_;
    doc["settingsVersion"] = settingsVersion_;
    JsonArray nightArr = doc.createNestedArray("nightWindows");
    for (uint8_t day = 0; day < 7; ++day) {
        JsonObject w = nightArr.createNestedObject();
        w["day"] = day;
        w["startMinute"] = nightStartMinuteByDay_[day];
        w["endMinute"] = nightEndMinuteByDay_[day];
    }
    JsonArray arr = doc.createNestedArray("alarms");

    for (size_t i = 0; i < alarmCount_; ++i) {
        JsonObject a = arr.createNestedObject();
        a["id"] = alarms_[i].id;
        a["hour"] = alarms_[i].hour;
        a["minute"] = alarms_[i].minute;
        a["daysMask"] = alarms_[i].daysMask;
        a["enabled"] = alarms_[i].enabled;
        a["volume"] = alarms_[i].volume;
        a["label"] = alarms_[i].label;
    }

    File f = LittleFS.open(storagePath_, "w");
    if (!f) {
        ALARM_LOG("save failed open %s", storagePath_.c_str());
        return;
    }

    serializeJson(doc, f);
    f.close();
}

void AlarmScheduler::evaluateAlarms() {
    static uint32_t lastEvalMs = 0;
    if ((millis() - lastEvalMs) < 500) {
        return;
    }
    lastEvalMs = millis();

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    time_t nowTs = time(nullptr);
    struct tm* ti = localtime(&nowTs);
    if (!ti || ti->tm_year < 120) {
        return;
    }

    const int minuteOfDay = ti->tm_hour * 60 + ti->tm_min;
    const int yDay = ti->tm_yday;
    const uint8_t dowMask = static_cast<uint8_t>(1U << ti->tm_wday);

    for (size_t i = 0; i < alarmCount_; ++i) {
        Alarm& a = alarms_[i];
        if (!a.enabled) continue;
        if ((a.daysMask & dowMask) == 0) continue;
        if (a.hour * 60 + a.minute != minuteOfDay) continue;
        if (a.lastTriggerYDay == yDay && a.lastTriggerMinute == minuteOfDay) continue;

        a.lastTriggerYDay = yDay;
        a.lastTriggerMinute = minuteOfDay;
        startRing(a.volume, a.label);
        ALARM_LOG("trigger id=%u %02u:%02u vol=%u", a.id, a.hour, a.minute, a.volume);
    }
}

void AlarmScheduler::startRing(uint8_t alarmVolume, const String& alarmLabel) {
    const uint16_t effective = static_cast<uint16_t>((static_cast<uint16_t>(alarmVolume) * masterVolume_) / 100U);
    ringOnMs_ = static_cast<uint16_t>(30 + (effective * 2));
    ringPeriodMs_ = 220;
    ringDurationMs_ = 30000;
    ringStartMs_ = millis();
    lastRingPulseMs_ = 0;
    ringingLabel_ = alarmLabel;
    ringingVolume_ = alarmVolume;
    ringing_ = true;
}

void AlarmScheduler::startSnooze(uint32_t snoozeMinutes, const String& alarmLabel) {
    snoozeActive_ = true;
    snoozeLabel_ = alarmLabel;
    snoozeDueMs_ = millis() + (snoozeMinutes * 60UL * 1000UL);
}

void AlarmScheduler::updateRing() {
    if (!ringing_) {
        return;
    }

    const uint32_t elapsed = millis() - ringStartMs_;
    if (elapsed > ringDurationMs_) {
        stopRing();
        return;
    }

    const uint32_t mod = elapsed % ringPeriodMs_;
    if (mod < ringOnMs_) {
        alarm_buzzer_on();
    } else {
        alarm_buzzer_off();
    }
}

void AlarmScheduler::stopRing() {
    if (!ringing_) {
        return;
    }
    ringing_ = false;
    ringingLabel_ = "";
    alarm_buzzer_off();
}

int AlarmScheduler::findAlarmIndexById(uint16_t id) const {
    for (size_t i = 0; i < alarmCount_; ++i) {
        if (alarms_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint8_t AlarmScheduler::clampToByte(int value, uint8_t minVal, uint8_t maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return static_cast<uint8_t>(value);
}

void AlarmScheduler::handleGetServiceWorker() {
    // Un simple Service Worker vide suffit pour valider les pré-requis d'installation de Chrome
    String sw = "self.addEventListener('fetch', function(event) {});";
    server_.send(200, "application/javascript", sw);
}

void AlarmScheduler::setupRoutes() {
    server_.on("/", HTTP_GET, [this]() { handleGetRoot(); });
    server_.on("/manifest.json", HTTP_GET, [this]() { handleGetManifest(); });
    server_.on("/sw.js", HTTP_GET, [this]() { handleGetServiceWorker(); });  
    
    server_.on("/api/state", HTTP_GET, [this]() { handleGetState(); });
    server_.on("/api/state", HTTP_GET, [this]() { handleGetState(); });
    server_.on("/api/alarms", HTTP_POST, [this]() { handleCreateAlarm(); });
    server_.on("/api/alarms", HTTP_PUT, [this]() { handleUpdateAlarm(); });
    server_.on("/api/alarms", HTTP_DELETE, [this]() { handleDeleteAlarm(); });
    server_.on("/api/volume", HTTP_POST, [this]() { handleSetVolume(); });
    server_.on("/api/enabled", HTTP_POST, [this]() { handleSetEnabled(); });
    server_.on("/api/night-window", HTTP_POST, [this]() { handleSetNightWindow(); });
    server_.onNotFound([this]() {
        server_.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
    });
}