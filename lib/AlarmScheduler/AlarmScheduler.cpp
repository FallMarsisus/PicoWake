#include "AlarmScheduler.h"

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

void AlarmScheduler::setupRoutes() {
    server_.on("/", HTTP_GET, [this]() { handleGetRoot(); });
    server_.on("/api/state", HTTP_GET, [this]() { handleGetState(); });
    server_.on("/api/alarms", HTTP_POST, [this]() { handleCreateAlarm(); });
    server_.on("/api/alarms", HTTP_PUT, [this]() { handleUpdateAlarm(); });
    server_.on("/api/alarms", HTTP_DELETE, [this]() { handleDeleteAlarm(); });
    server_.on("/api/volume", HTTP_POST, [this]() { handleSetVolume(); });
    server_.on("/api/enabled", HTTP_POST, [this]() { handleSetEnabled(); });
    server_.onNotFound([this]() {
        server_.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
    });
}

String AlarmScheduler::buildAppPage() const {
    String html;
    html.reserve(9000);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>PicoWake Alarmes</title>";
    html += "<style>";
    html += ":root{--bg:#f4f7fb;--card:#ffffff;--ink:#0f172a;--muted:#64748b;--accent:#0ea5e9;--danger:#ef4444;--ok:#16a34a;}";
    html += "body{margin:0;font-family:ui-sans-serif,system-ui,sans-serif;background:linear-gradient(180deg,#f4f7fb,#eaf2ff);color:var(--ink);}";
    html += ".wrap{max-width:760px;margin:0 auto;padding:16px;}";
    html += ".card{background:var(--card);border-radius:16px;padding:14px;box-shadow:0 8px 24px rgba(15,23,42,.08);margin-bottom:12px;}";
    html += "h1{font-size:1.25rem;margin:0 0 12px;} h2{font-size:1rem;margin:0 0 10px;}";
    html += "label{font-size:.9rem;color:var(--muted);display:block;margin-bottom:6px;}";
    html += "input,button,select{font:inherit;}";
    html += "input[type='time'],input[type='text']{width:100%;box-sizing:border-box;padding:10px;border:1px solid #d7e1f0;border-radius:10px;}";
    html += "button{border:0;border-radius:10px;padding:10px 12px;font-weight:700;background:var(--accent);color:white;}";
    html += "button.ghost{background:#e2e8f0;color:#0f172a;} button.danger{background:var(--danger);} .row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;}";
    html += ".days{display:grid;grid-template-columns:repeat(7,minmax(0,1fr));gap:6px;} .day{padding:7px;border:1px solid #cbd5e1;border-radius:8px;text-align:center;font-size:.82rem;}";
    html += ".alarm{display:flex;justify-content:space-between;gap:8px;align-items:center;border:1px solid #e2e8f0;border-radius:12px;padding:10px;margin-bottom:8px;}";
    html += ".time{font-size:1.4rem;font-weight:800;} .meta{font-size:.85rem;color:var(--muted);} .badge{padding:3px 8px;border-radius:999px;font-size:.72rem;}";
    html += ".on{background:#dcfce7;color:#166534;} .off{background:#fee2e2;color:#991b1b;} .slider{width:100%;}";
    html += "</style></head><body><div class='wrap'>";
    html += "<h1>Reveils PicoWake</h1>";
    html += "<div class='card'><h2>Volume sonnerie</h2><input id='vol' class='slider' type='range' min='0' max='100'><div id='volTxt' class='meta'></div></div>";
    html += "<div class='card'><h2>Ajouter un reveil</h2>";
    html += "<label>Heure</label><input id='time' type='time' value='07:00'>";
    html += "<label style='margin-top:10px;'>Nom (optionnel)</label><input id='label' type='text' placeholder='Ex: Travail'>";
    html += "<label style='margin-top:10px;'>Jours</label><div class='days' id='days'></div>";
    html += "<div class='row' style='margin-top:12px;'><button id='addBtn'>Ajouter</button></div></div>";
    html += "<div class='card'><h2>Mes reveils</h2><div id='list'></div></div>";
    html += "</div><script>";
    html += "const dayNames=['Dim','Lun','Mar','Mer','Jeu','Ven','Sam'];";
    html += "const daysEl=document.getElementById('days');";
    html += "const state={alarms:[],volume:70};";
    html += "for(let i=0;i<7;i++){const l=document.createElement('label');l.className='day';l.innerHTML=`<input type='checkbox' data-day='${i}' ${(i>=1&&i<=5)?'checked':''}> ${dayNames[i]}`;daysEl.appendChild(l);} ";
    html += "async function api(path,opt={}){const r=await fetch(path,{headers:{'Content-Type':'application/json'},...opt});if(!r.ok)throw new Error(await r.text());return r.json();}";
    html += "function maskToText(m){const out=[];for(let i=0;i<7;i++){if(m&(1<<i))out.push(dayNames[i]);}return out.length?out.join(' '):'Aucun jour';}";
    html += "function render(){document.getElementById('vol').value=state.volume;document.getElementById('volTxt').textContent=`${state.volume}%`;const list=document.getElementById('list');list.innerHTML='';";
    html += "state.alarms.sort((a,b)=>(a.hour*60+a.minute)-(b.hour*60+b.minute));";
    html += "for(const a of state.alarms){const el=document.createElement('div');el.className='alarm';const hh=String(a.hour).padStart(2,'0');const mm=String(a.minute).padStart(2,'0');";
    html += "el.innerHTML=`<div><div class='time'>${hh}:${mm}</div><div class='meta'>${a.label||'Sans nom'} - ${maskToText(a.daysMask)}</div></div><div class='row'><span class='badge ${a.enabled?'on':'off'}'>${a.enabled?'Actif':'Off'}</span><button class='ghost' data-toggle='${a.id}'>${a.enabled?'Desactiver':'Activer'}</button><button class='danger' data-del='${a.id}'>Suppr</button></div>`;list.appendChild(el);} }";
    html += "async function refresh(){const s=await api('/api/state');state.alarms=s.alarms||[];state.volume=s.volume||70;render();}";
    html += "document.getElementById('addBtn').onclick=async()=>{const t=document.getElementById('time').value||'07:00';const [h,m]=t.split(':').map(Number);let mask=0;document.querySelectorAll('#days input').forEach(ch=>{if(ch.checked)mask|=(1<<Number(ch.dataset.day));});const label=document.getElementById('label').value||'';await api('/api/alarms',{method:'POST',body:JSON.stringify({hour:h,minute:m,daysMask:mask,label,enabled:true})});document.getElementById('label').value='';await refresh();};";
    html += "document.getElementById('vol').oninput=async(e)=>{const value=Number(e.target.value);state.volume=value;document.getElementById('volTxt').textContent=`${value}%`;await api('/api/volume',{method:'POST',body:JSON.stringify({value})});};";
    html += "document.getElementById('list').onclick=async(e)=>{const t=e.target;if(t.dataset.del){await api('/api/alarms?id='+t.dataset.del,{method:'DELETE'});await refresh();}if(t.dataset.toggle){const id=Number(t.dataset.toggle);const a=state.alarms.find(x=>x.id===id);await api('/api/enabled',{method:'POST',body:JSON.stringify({id,enabled:!a.enabled})});await refresh();}};";
    html += "refresh();setInterval(refresh,8000);";
    html += "</script></body></html>";
    return html;
}

void AlarmScheduler::handleGetRoot() {
    server_.send(200, "text/html; charset=utf-8", buildAppPage());
}

void AlarmScheduler::handleGetState() {
    DynamicJsonDocument doc(4096);
    doc["ok"] = true;
    doc["volume"] = masterVolume_;
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

void AlarmScheduler::loadFromDisk() {
    alarmCount_ = 0;
    nextId_ = 1;
    masterVolume_ = 70;

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

    ALARM_LOG("loaded alarms=%u volume=%u", static_cast<unsigned>(alarmCount_), static_cast<unsigned>(masterVolume_));
}

void AlarmScheduler::saveToDisk() {
    DynamicJsonDocument doc(6144);
    doc["masterVolume"] = masterVolume_;
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
