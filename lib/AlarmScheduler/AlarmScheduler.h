#ifndef ALARM_SCHEDULER_H
#define ALARM_SCHEDULER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

class AlarmScheduler {
public:
    void begin(const char* storagePath = "/alarms.json");
    void loop();
    void ensureWebStarted(const char* mdnsHost);
    void stopWeb();
    bool isWebRunning() const;
    bool isRinging() const;
    void dismissRinging();
    void snoozeRinging(uint32_t snoozeMinutes = 10);
    size_t activeEnabledCount() const;
    String currentRingingLabel() const;
    bool hasSnooze() const;
    uint32_t snoozeRemainingSeconds() const;
    uint16_t nightStartMinuteForDay(uint8_t dayIndex) const;
    uint16_t nightEndMinuteForDay(uint8_t dayIndex) const;
    uint32_t settingsVersion() const;

private:
    struct Alarm {
        uint16_t id = 0;
        uint8_t hour = 7;
        uint8_t minute = 0;
        uint8_t daysMask = 0x7F; // bit0=Sunday ... bit6=Saturday
        bool enabled = true;
        uint8_t volume = 70;
        String label;
        int lastTriggerYDay = -1;
        int lastTriggerMinute = -1;
    };

    WebServer server_{80};
    String storagePath_;
    bool webRunning_ = false;
    uint16_t nextId_ = 1;
    uint8_t masterVolume_ = 70;
    uint16_t nightStartMinuteByDay_[7] = {1260, 1260, 1260, 1260, 1260, 1260, 1260};
    uint16_t nightEndMinuteByDay_[7] = {420, 420, 420, 420, 420, 420, 420};
    uint32_t settingsVersion_ = 1;

    static constexpr size_t kMaxAlarms = 24;
    Alarm alarms_[kMaxAlarms];
    size_t alarmCount_ = 0;

    bool ringing_ = false;
    String ringingLabel_;
    uint8_t ringingVolume_ = 70;
    bool snoozeActive_ = false;
    String snoozeLabel_;
    uint32_t snoozeDueMs_ = 0;
    uint32_t ringStartMs_ = 0;
    uint32_t lastRingPulseMs_ = 0;
    uint16_t ringOnMs_ = 100;
    uint16_t ringPeriodMs_ = 420;
    uint16_t ringDurationMs_ = 30000;

    void setupRoutes();
    String buildAppPage() const;

    void handleGetRoot();
    void handleGetState();
    void handleCreateAlarm();
    void handleUpdateAlarm();
    void handleDeleteAlarm();
    void handleSetVolume();
    void handleSetEnabled();
    void handleSetNightWindow();

    void loadFromDisk();
    void saveToDisk();

    void evaluateAlarms();
    void startRing(uint8_t alarmVolume, const String& alarmLabel);
    void startSnooze(uint32_t snoozeMinutes, const String& alarmLabel);
    void updateRing();
    void stopRing();

    int findAlarmIndexById(uint16_t id) const;
    static uint8_t clampToByte(int value, uint8_t minVal, uint8_t maxVal);
};

#endif
