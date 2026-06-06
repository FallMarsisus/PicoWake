#include <Arduino.h>
#include "Hardware.h"
#include "FirstWifiProvisioning.h"
#include <WiFiNTP.h>
#include <HTTPClient.h>
#include <time.h>
#include <qrcode.h>
#include <ArduinoJson.h>
#include "assets/ronds.h"
#include "AlarmScheduler.h"

#include "assets/jgs_Font16pt7b.h"  
#include "assets/jgs_Font30pt7b.h"
#include "assets/jgs_Font40pt7b.h"


static const unsigned char PROGMEM image_clock_alarm_bits[] = {0x79,0x3c,0xb3,0x9a,0xed,0x6e,0xd0,0x16,0xa0,0x0a,0x41,0x04,0x41,0x04,0x81,0x02,0xc1,0x06,0x82,0x02,0x44,0x04,0x48,0x04,0x20,0x08,0x10,0x10,0x2d,0x68,0x43,0x84};
static const unsigned char PROGMEM image_device_sleep_mode_white_bits[] = {0x04,0x00,0x1c,0x0e,0x28,0x02,0x48,0x04,0x51,0xee,0x90,0x40,0x90,0x80,0x91,0xe0,0x88,0x00,0x88,0x06,0x46,0x1c,0x41,0xe4,0x20,0x08,0x18,0x30,0x07,0xc0,0x00,0x00};
static const unsigned char PROGMEM image_display_brightness_bits[] = {0x01,0x00,0x21,0x08,0x10,0x10,0x03,0x80,0x8c,0x62,0x48,0x24,0x10,0x10,0x10,0x10,0x10,0x10,0x48,0x24,0x8c,0x62,0x03,0x80,0x10,0x10,0x21,0x08,0x01,0x00,0x00,0x00};
static const unsigned char PROGMEM image_wifi_bits[] = {0x01,0xf0,0x00,0x06,0x0c,0x00,0x18,0x03,0x00,0x21,0xf0,0x80,0x46,0x0c,0x40,0x88,0x02,0x20,0x10,0xe1,0x00,0x23,0x18,0x80,0x04,0x04,0x00,0x08,0x42,0x00,0x01,0xb0,0x00,0x02,0x08,0x00,0x00,0x40,0x00,0x00,0xa0,0x00,0x00,0x40,0x00,0x00,0x00,0x00};
static const unsigned char PROGMEM image_wifi_not_connected_bits[] = {0x21,0xf0,0x00,0x16,0x0c,0x00,0x08,0x03,0x00,0x25,0xf0,0x80,0x42,0x0c,0x40,0x89,0x02,0x20,0x10,0xa1,0x00,0x23,0x58,0x80,0x04,0x24,0x00,0x08,0x52,0x00,0x01,0xa8,0x00,0x02,0x04,0x00,0x00,0x42,0x00,0x00,0xa1,0x00,0x00,0x40,0x80,0x00,0x00,0x00};

static constexpr int kWifiIconX = 5;
static constexpr int kWifiIconY = 5;
static constexpr int kWifiIconW = 19;
static constexpr int kWifiIconH = 16;


static constexpr bool kBypassWifiProvisioning = false;
static constexpr const char* kApSsid = "PicoWake-Setup";
static constexpr const char* kApPass = "picowake123";
static constexpr const char* kPortalUrl = "http://192.168.4.1";
// "ffo62" interprete comme "ff062" (o -> 0), soit RGB(15,240,98)
static constexpr uint16_t kClockBgColor = 0xc0e5;
// Rouge fonce (RGB approx 110, 0, 0)
static constexpr uint16_t kNightBgColor = 0x0000;
// Rose custom (RGB approx 255,80,150)
static constexpr uint16_t kSnoozePinkBgColor = 0x0000;
static constexpr uint8_t kBacklightDay = 255;
static constexpr uint8_t kBacklightNight = 28;
static constexpr uint8_t kBacklightSnooze = 18;

static FirstWifiProvisioning wifiProvisioning;
static AlarmScheduler alarmScheduler;
static bool sLastTouchState = false;
static bool sTimeInitialized = false;
static bool sNtpStarted = false;
static uint32_t sLastNtpAttemptMs = 0;
static uint32_t sLastWifiRetryMs = 0;
static uint8_t sWifiRetryCount = 0;

static constexpr uint32_t kWifiRetryIntervalMs = 2500;
static constexpr uint8_t kWifiRetryBeforePortal = 3;
static constexpr uint32_t kNtpRetryIntervalMs = 10000;

enum class UiScreen {
    Loading,
    Clock,
    WifiHelp,
    AlarmRinging,
    Weather
};

static UiScreen sCurrentScreen = UiScreen::Loading;
static bool sScreenDirty = true;
static bool sWifiHelpPortalPrimed = false;
static String sLastClock;
static int sLastActiveCount = -1;
static uint32_t sLastLoadingAnimMs = 0;
static uint8_t sLoadingFrame = 0;

static uint32_t sLoopCount = 0;
static uint32_t sMaxLoopDeltaMs = 0;
static uint32_t sLastSchedulerSettingsVersion = 0;
static uint32_t sSnoozePinkUntilMs = 0;
static uint16_t sClockBgColorActive = kClockBgColor;
static uint8_t sCurrentBacklight = kBacklightDay;
static uint8_t sTargetBacklight = kBacklightDay;
static uint32_t sLastBacklightStepMs = 0;

enum class ClockTheme {
    Day,
    Night,
    SnoozePink,
    ScreenOff,
};

static ClockTheme sCurrentTheme = ClockTheme::Day;
static ClockTheme sVisualTheme = ClockTheme::Day;

static bool sGeoResolved = false;
static float sGeoLat = 0.0f;
static float sGeoLon = 0.0f;
static int sSunriseMinute = 7 * 60;
static int sSunsetMinute = 20 * 60;
static int sWeatherCode = -1;
static float sTempMax = -99.0f;
static float sTempMin = -99.0f;
static uint32_t sWeatherDisplayUntilMs = 0; // Timer des 10 minutes
static int sSunTimesYDay = -1;
static bool sSunTimesValid = false;
static uint32_t sLastSunFetchAttemptMs = 0;

static constexpr uint32_t kSunFetchRetryMs = 30000;
static constexpr uint32_t kSnoozePinkDurationMs = 10000;
static constexpr uint32_t kBacklightFadeStepMs = 16;
static constexpr uint8_t kBacklightFadeStep = 4;

static int parseIsoMinute(const String& isoDateTime) {
    const int tPos = isoDateTime.indexOf('T');
    if (tPos < 0 || tPos + 5 >= isoDateTime.length()) {
        return -1;
    }

    const int h = isoDateTime.substring(tPos + 1, tPos + 3).toInt();
    const int m = isoDateTime.substring(tPos + 4, tPos + 6).toInt();
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        return -1;
    }
    return h * 60 + m;
}

static bool fetchGeoFromInternet() {
    HTTPClient http;
    if (!http.begin("http://ip-api.com/json/?fields=status,lat,lon")) {
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        return false;
    }

    if (String((const char*)(doc["status"] | "")) != "success") {
        return false;
    }

    sGeoLat = doc["lat"] | 0.0f;
    sGeoLon = doc["lon"] | 0.0f;
    sGeoResolved = true;
    Serial.printf("[SUN] geo resolved lat=%.4f lon=%.4f\n", sGeoLat, sGeoLon);
    return true;
}

static bool fetchSunTimesFromInternet() {
    if (!sGeoResolved) {
        return false;
    }

    String url = "http://api.open-meteo.com/v1/forecast?latitude=";
    url += String(sGeoLat, 6);
    url += "&longitude=";
    url += String(sGeoLon, 6);
    url += "&daily=sunrise,sunset,weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1";

    HTTPClient http;
    if (!http.begin(url)) {
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        return false;
    }

    JsonArray sunriseArray = doc["daily"]["sunrise"].as<JsonArray>();
    JsonArray sunsetArray = doc["daily"]["sunset"].as<JsonArray>();
    if (sunriseArray.isNull() || sunsetArray.isNull() || sunriseArray.size() == 0 || sunsetArray.size() == 0) {
        return false;
    }

    const String sunriseIso = sunriseArray[0].as<String>();
    const String sunsetIso = sunsetArray[0].as<String>();
    const int sunriseMinute = parseIsoMinute(sunriseIso);
    const int sunsetMinute = parseIsoMinute(sunsetIso);
    if (sunriseMinute < 0 || sunsetMinute < 0 || sunriseMinute >= sunsetMinute) {
        return false;
    }

    sSunriseMinute = sunriseMinute;
    sSunsetMinute = sunsetMinute;
    
    // Utilise "weather_code" pour correspondre à la documentation de l'API
    sWeatherCode = doc["daily"]["weather_code"][0] | -1;
    sTempMax = doc["daily"]["temperature_2m_max"][0] | -99.0f;
    sTempMin = doc["daily"]["temperature_2m_min"][0] | -99.0f;
    // ----------------------------------------
    sSunTimesValid = true;
    const time_t nowTs = time(nullptr);
    const struct tm* ti = localtime(&nowTs);
    if (ti) {
        sSunTimesYDay = ti->tm_yday;
    }
    Serial.printf("[SUN] updated sunrise=%02d:%02d sunset=%02d:%02d\n",
                  sSunriseMinute / 60,
                  sSunriseMinute % 60,
                  sSunsetMinute / 60,
                  sSunsetMinute % 60);
    return true;
}

static String getWeatherDesc(int code) {
    if (code < 0) return "Inconnu";
    if (code == 0) return "Soleil";
    if (code >= 1 && code <= 3) return "Nuageux";
    if (code == 45 || code == 48) return "Brouillard";
    if (code >= 51 && code <= 67) return "Pluie";
    if (code >= 71 && code <= 77) return "Neige";
    if (code >= 80 && code <= 82) return "Averses";
    if (code >= 95) return "Orage";
    return "Meteo instable";
}

static bool refreshSunTimesIfNeeded(uint32_t nowMs) {
    if (!wifiProvisioning.isConnected() || !sTimeInitialized) {
        return false;
    }

    const time_t nowTs = time(nullptr);
    const struct tm* ti = localtime(&nowTs);
    if (!ti || ti->tm_year < 120) {
        return false;
    }

    const bool dayChanged = (ti->tm_yday != sSunTimesYDay);
    const bool needRefresh = !sSunTimesValid || dayChanged;
    if (!needRefresh) {
        return false;
    }

    if ((nowMs - sLastSunFetchAttemptMs) < kSunFetchRetryMs) {
        return false;
    }
    sLastSunFetchAttemptMs = nowMs;

    if (!sGeoResolved && !fetchGeoFromInternet()) {
        Serial.println("[SUN] geo fetch failed, will retry");
        return false;
    }

    if (!fetchSunTimesFromInternet()) {
        Serial.println("[SUN] sunrise/sunset fetch failed, will retry");
        return false;
    }

    return true;
}

static void drawWifiStatusIcon(bool force) {
    static int sLastConnected = -1;
    static uint16_t sLastBg = 0;
    const bool wifiConnected = wifiProvisioning.isConnected();
    if (!force && sLastConnected == (wifiConnected ? 1 : 0) && sLastBg == sClockBgColorActive) {
        return;
    }

    tft.fillRect(kWifiIconX, kWifiIconY, kWifiIconW, kWifiIconH, sClockBgColorActive);
    const unsigned char* wifiIcon = wifiConnected ? image_wifi_bits : image_wifi_not_connected_bits;
    const uint16_t iconColor = wifiConnected ? TFT_WHITE : TFT_RED;
    tft.drawBitmap(kWifiIconX, kWifiIconY, wifiIcon, kWifiIconW, kWifiIconH, iconColor);

    sLastConnected = wifiConnected ? 1 : 0;
    sLastBg = sClockBgColorActive;
}

static bool isNightTime(const struct tm* ti) {
    if (!ti) {
        return false;
    }

    const uint8_t dayIndex = static_cast<uint8_t>(ti->tm_wday % 7);
    const uint8_t prevDayIndex = static_cast<uint8_t>((dayIndex + 6) % 7);
    const int nowMinute = ti->tm_hour * 60 + ti->tm_min;
    const int todayStart = static_cast<int>(alarmScheduler.nightStartMinuteForDay(dayIndex));
    const int todayEnd = static_cast<int>(alarmScheduler.nightEndMinuteForDay(dayIndex));
    const int prevStart = static_cast<int>(alarmScheduler.nightStartMinuteForDay(prevDayIndex));
    const int prevEnd = static_cast<int>(alarmScheduler.nightEndMinuteForDay(prevDayIndex));

    // Semantics: row "Sam-Dim" means start Saturday evening and end Sunday morning.
    if (todayStart != todayEnd && nowMinute >= todayStart) {
        return true;
    }

    if (prevStart != prevEnd && nowMinute < prevEnd) {
        return true;
    }

    return false;
}

static bool isScreenOffSchedule(const struct tm* ti) {
    if (!ti) {
        return false;
    }
    // Screen-off window now follows the scheduler night window per day.
    return isNightTime(ti);
}

static void applyBacklightImmediate(uint8_t level) {
    analogWrite(PIN_TFT_BL, level);
    sCurrentBacklight = level;
    sTargetBacklight = level;
}

static void setBacklightTarget(uint8_t level) {
    sTargetBacklight = level;
}

static void serviceBacklightFade(uint32_t nowMs) {
    if (sCurrentBacklight == sTargetBacklight) {
        return;
    }

    if ((nowMs - sLastBacklightStepMs) < kBacklightFadeStepMs) {
        return;
    }
    sLastBacklightStepMs = nowMs;

    if (sCurrentBacklight < sTargetBacklight) {
        const uint16_t next = static_cast<uint16_t>(sCurrentBacklight) + kBacklightFadeStep;
        sCurrentBacklight = (next > sTargetBacklight) ? sTargetBacklight : static_cast<uint8_t>(next);
    } else {
        const int next = static_cast<int>(sCurrentBacklight) - static_cast<int>(kBacklightFadeStep);
        sCurrentBacklight = (next < static_cast<int>(sTargetBacklight)) ? sTargetBacklight : static_cast<uint8_t>(next);
    }

    analogWrite(PIN_TFT_BL, sCurrentBacklight);
}

static bool setVisualTheme(ClockTheme visualTheme) {
    if (sVisualTheme == visualTheme) {
        return false;
    }

    sVisualTheme = visualTheme;
    if (sVisualTheme == ClockTheme::SnoozePink) {
        sClockBgColorActive = kSnoozePinkBgColor;
    } else if (sVisualTheme == ClockTheme::Night) {
        sClockBgColorActive = kNightBgColor;
    } else if (sVisualTheme == ClockTheme::Day) {
        sClockBgColorActive = kClockBgColor;
    } else {
        sClockBgColorActive = TFT_BLACK;
    }
    return true;
}

static bool updateClockTheme() {
    const uint32_t nowMs = millis();
    const time_t nowTs = time(nullptr);
    const struct tm* ti = localtime(&nowTs);
    const bool pinkActive = (sSnoozePinkUntilMs > nowMs);
    const bool timeReady = sTimeInitialized && ti && ti->tm_year > 100;

    ClockTheme nextTheme = ClockTheme::Day;
    // Snooze press must temporarily wake the screen, even during screen-off schedule.
    if (pinkActive) {
        nextTheme = ClockTheme::SnoozePink;
    } else if (timeReady && isScreenOffSchedule(ti)) {
        nextTheme = ClockTheme::ScreenOff;
    }

    bool changed = false;
    if (nextTheme != sCurrentTheme) {
        sCurrentTheme = nextTheme;
        changed = true;
    }

    if (sCurrentTheme == ClockTheme::ScreenOff) {
        // Keep current visual content and only fade backlight to 0 to avoid hard black redraw artifacts.
        setBacklightTarget(0);
        return changed;
    }

    if (sCurrentTheme == ClockTheme::Night) {
        changed = setVisualTheme(ClockTheme::Night) || changed;
        setBacklightTarget(kBacklightNight);
    } else if (sCurrentTheme == ClockTheme::SnoozePink) {
        changed = setVisualTheme(ClockTheme::SnoozePink) || changed;
        setBacklightTarget(kBacklightSnooze);
    } else {
        changed = setVisualTheme(ClockTheme::Day) || changed;
        setBacklightTarget(kBacklightDay);
    }

    return changed;
}

static bool systemTimeIsValid() {
    const time_t nowTs = time(nullptr);
    const struct tm* timeinfo = localtime(&nowTs);
    return timeinfo && timeinfo->tm_year > 100;
}

static void drawCentered(const String& text, int y, int font, uint16_t fg, uint16_t bg = TFT_BLACK) {
    tft.setTextColor(fg, bg);
    tft.drawCentreString(text, tft.width() / 2, y, font);
}

static void drawQrCode(const char* payload, int x0, int y0, int scale = 4) {
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    QRCode qrcode;
    qrcode_initText(&qrcode, qrcodeData, 4, ECC_MEDIUM, payload);

    const int size = qrcode.size;
    const int qrPixelSize = size * scale;

    tft.fillRect(x0 - 4, y0 - 4, qrPixelSize + 8, qrPixelSize + 8, TFT_WHITE);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qrcode_getModule(&qrcode, x, y)) {
                tft.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, TFT_BLACK);
            }
        }
    }
}

static void drawInfoPill(int x, int y, int w, int h, const String& text, uint16_t fg, uint16_t bg, uint16_t border) {
    tft.fillRoundRect(x, y, w, h, 10, bg);
    tft.drawRoundRect(x, y, w, h, 10, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(text, x + w / 2, y + h / 2 + 1, 2);
}

static void drawLoadingScreen(bool force) {
    static const int8_t kDotX[8] = {0, 10, 14, 10, 0, -10, -14, -10};
    static const int8_t kDotY[8] = {-14, -10, 0, 10, 14, 10, 0, -10};
    const int cx = tft.width() / 2;
    const int cy = tft.height() / 2;

    if (force) {
        tft.fillScreen(TFT_BLACK);
        sLoadingFrame = 0;
        sLastLoadingAnimMs = 0;
    }

    if ((millis() - sLastLoadingAnimMs) > 120) {
        sLastLoadingAnimMs = millis();
        sLoadingFrame = (sLoadingFrame + 1) % 100;

        tft.fillRect(cx - 20, cy - 20, 40, 40, TFT_BLACK);
        for (uint8_t i = 0; i < 8; ++i) {
            const uint8_t idx = (sLoadingFrame / 2 + i) % 8;
            const uint16_t color = (i == 0) ? TFT_CYAN : ((i <= 2) ? TFT_DARKCYAN : TFT_DARKGREY);
            tft.fillCircle(cx + kDotX[idx], cy + kDotY[idx], 3, color);
        }
    }
}

static String formatSnoozeText(uint32_t snoozeRemaining) {
    const uint32_t minutes = snoozeRemaining / 60U;
    const uint32_t seconds = snoozeRemaining % 60U;
    char snoozeBuf[16];
    if (minutes >= 10U) {
        snprintf(snoozeBuf, sizeof(snoozeBuf), "%02lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
    } else {
        snprintf(snoozeBuf, sizeof(snoozeBuf), "%lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
    }
    return String(snoozeBuf);
}

static void drawSnoozeOverlay(bool force, uint32_t snoozeRemaining) {
    static int lastOverlayX = -1;
    static int lastOverlayY = -1;
    static int lastOverlayW = -1;
    static int lastOverlayH = -1;

    const int w = tft.width();
    const int h = tft.height();

    if (force || snoozeRemaining == 0) {
        if (lastOverlayW > 0 && lastOverlayH > 0) {
            tft.fillRoundRect(lastOverlayX - 2, lastOverlayY - 2, lastOverlayW + 4, lastOverlayH + 4, 12, sClockBgColorActive);
        }
        lastOverlayX = -1;
        lastOverlayY = -1;
        lastOverlayW = -1;
        lastOverlayH = -1;
        return;
    }

    const String snoozeText = formatSnoozeText(snoozeRemaining);
    const int textW = tft.textWidth(snoozeText, 2);
    const int iconW = 24;
    const int gap = 8;
    const int paddingX = 12;
    const int barH = 32;
    int barW = iconW + gap + textW + (paddingX * 2);
    if (barW < 92) {
        barW = 92;
    }
    if (barW > 156) {
        barW = 156;
    }

    const int barX = (w - barW) / 2;
    const int barY = h - barH - 8;

    if (lastOverlayW > 0 && lastOverlayH > 0) {
        tft.fillRoundRect(lastOverlayX - 2, lastOverlayY - 2, lastOverlayW + 4, lastOverlayH + 4, 12, sClockBgColorActive);
    }

    tft.fillRoundRect(barX, barY, barW, barH, 12, TFT_BLACK);
    tft.drawRoundRect(barX, barY, barW, barH, 12, TFT_DARKGREY);
    tft.drawBitmap(barX + paddingX, barY + 4, alarm, 24, 24, TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(snoozeText, barX + paddingX + iconW + gap, barY + 9, 2);

    lastOverlayX = barX;
    lastOverlayY = barY;
    lastOverlayW = barW;
    lastOverlayH = barH;
}

static void drawWeatherIcon(int cx, int cy, int code) {
    if (code < 0) return; // Inconnu
    
    if (code == 0) { 
        // ☀️ Soleil radieux
        tft.fillCircle(cx, cy, 14, TFT_YELLOW);
        tft.drawLine(cx, cy - 17, cx, cy - 24, TFT_YELLOW);
        tft.drawLine(cx, cy + 17, cx, cy + 24, TFT_YELLOW);
        tft.drawLine(cx - 17, cy, cx - 24, cy, TFT_YELLOW);
        tft.drawLine(cx + 17, cy, cx + 24, cy, TFT_YELLOW);
        tft.drawLine(cx - 12, cy - 12, cx - 17, cy - 17, TFT_YELLOW);
        tft.drawLine(cx + 12, cy + 12, cx + 17, cy + 17, TFT_YELLOW);
        tft.drawLine(cx - 12, cy + 12, cx - 17, cy + 17, TFT_YELLOW);
        tft.drawLine(cx + 12, cy - 12, cx + 17, cy - 17, TFT_YELLOW);
        
    } else if (code >= 1 && code <= 3) { 
        // ⛅ Nuageux / Éclaircies
        if (code == 1 || code == 2) { 
            tft.fillCircle(cx - 10, cy - 10, 12, TFT_YELLOW); // Soleil caché
        }
        tft.fillCircle(cx - 12, cy + 4, 10, TFT_LIGHTGREY);
        tft.fillCircle(cx + 12, cy + 4, 10, TFT_LIGHTGREY);
        tft.fillCircle(cx, cy - 4, 14, TFT_WHITE);
        tft.fillRect(cx - 12, cy - 6, 24, 20, TFT_WHITE);
        
    } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) { 
        // 🌧️ Pluie
        tft.fillCircle(cx - 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx + 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx, cy - 8, 14, TFT_LIGHTGREY);
        tft.fillRect(cx - 12, cy - 10, 24, 20, TFT_LIGHTGREY);
        tft.drawLine(cx - 10, cy + 12, cx - 14, cy + 22, TFT_CYAN);
        tft.drawLine(cx,      cy + 12, cx - 4,  cy + 22, TFT_CYAN);
        tft.drawLine(cx + 10, cy + 12, cx + 6,  cy + 22, TFT_CYAN);
        
    } else if (code >= 71 && code <= 77) { 
        // ❄️ Neige
        tft.fillCircle(cx - 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx + 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx, cy - 8, 14, TFT_LIGHTGREY);
        tft.fillRect(cx - 12, cy - 10, 24, 20, TFT_LIGHTGREY);
        tft.fillCircle(cx - 10, cy + 16, 2, TFT_WHITE);
        tft.fillCircle(cx,      cy + 16, 2, TFT_WHITE);
        tft.fillCircle(cx + 10, cy + 16, 2, TFT_WHITE);
        
    } else if (code >= 95) { 
        // ⛈️ Orage
        tft.fillCircle(cx - 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx + 12, cy, 10, TFT_DARKGREY);
        tft.fillCircle(cx, cy - 8, 12, TFT_DARKGREY);
        tft.fillRect(cx - 12, cy - 10, 24, 20, TFT_DARKGREY);
        tft.drawLine(cx, cy + 5, cx - 6, cy + 15, TFT_YELLOW);
        tft.drawLine(cx - 6, cy + 15, cx + 2, cy + 15, TFT_YELLOW);
        tft.drawLine(cx + 2, cy + 15, cx - 4, cy + 25, TFT_YELLOW);
        
    } else { 
        // 🌫️ Brouillard
        tft.drawFastHLine(cx - 15, cy - 5, 30, TFT_LIGHTGREY);
        tft.drawFastHLine(cx - 10, cy + 2, 20, TFT_LIGHTGREY);
        tft.drawFastHLine(cx - 18, cy + 9, 36, TFT_LIGHTGREY);
    }
}

static void drawWeatherScreen(bool force) {
    static String lastClockWeather = "";
    static int lastWeatherCode = -999;
    static int lastActiveCount = -1;
    
    String clockText = "--:--";
    const time_t nowTs = time(nullptr);
    const struct tm* timeinfo = localtime(&nowTs);
    
    if (timeinfo && timeinfo->tm_year > 100) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
        clockText = String(buf);
    }

    const int activeCount = static_cast<int>(alarmScheduler.activeEnabledCount());
    bool needRedraw = force || clockText != lastClockWeather || sWeatherCode != lastWeatherCode || activeCount != lastActiveCount;

    if (needRedraw) {
        // -- FOND --
        // Haut (0 à 118) : Thème habituel
        tft.fillRect(0, 0, tft.width(), 118, sClockBgColorActive);
        // Bas (118 à 240) : Fond noir
        tft.fillRect(0, 118, tft.width(), 125, TFT_BLACK);
        
        // Séparation
        tft.drawFastHLine(0, 118, tft.width(), TFT_DARKGREY);
        tft.drawFastHLine(0, 119, tft.width(), TFT_DARKGREY);

        // -- HAUT : HEURE (Plus petite) --
        tft.setTextColor(TFT_WHITE, sClockBgColorActive);
        tft.setFreeFont(&jgs_Font30pt7b); // Police 30pt au lieu de 40pt
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM); 
        tft.drawString(clockText, tft.width() / 2, 58);

        // -- HAUT : ALARMES (Haut à droite) --
        tft.setFreeFont(0);
        tft.setTextSize(2);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(String(activeCount), 295, 4);
        tft.drawBitmap(301, 4, image_clock_alarm_bits, 15, 16, TFT_WHITE);

        // -- BAS : METEO --
        if (sTempMax > -90.0f) {
            // Icône Météo Vectorielle sur la gauche
            drawWeatherIcon(50, 175, sWeatherCode);

            tft.setFreeFont(0);
            tft.setTextSize(2); 
            tft.setTextDatum(TL_DATUM); // Alignement à gauche
            
            // Description météo (à droite de l'icône)
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(getWeatherDesc(sWeatherCode), 100, 145);

            // Température Min (Bleu)
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            char bufMin[16];
            snprintf(bufMin, sizeof(bufMin), "Min: %.1f", sTempMin);
            tft.drawString(bufMin, 100, 170);

            // Température Max (Orange)
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            char bufMax[16];
            snprintf(bufMax, sizeof(bufMax), "Max: %.1f", sTempMax);
            tft.drawString(bufMax, 100, 195); 
            
        } else {
            tft.setFreeFont(0);
            tft.setTextSize(2);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString("Recherche en cours...", tft.width() / 2, 175);
        }

        lastClockWeather = clockText;
        lastWeatherCode = sWeatherCode;
        lastActiveCount = activeCount;
    }
    
    // -- HAUT : WIFI (Haut à gauche) --
    drawWifiStatusIcon(needRedraw);
}

static void drawClockScreen(bool force, const String& clockText) {
    const int activeCount = static_cast<int>(alarmScheduler.activeEnabledCount());
    const bool snoozeVisible = alarmScheduler.hasSnooze();
    const uint32_t snoozeRemaining = snoozeVisible ? alarmScheduler.snoozeRemainingSeconds() : 0;
    static int lastSnoozeRemaining = -1;
    static bool lastSnoozeVisible = false;

    if (force) {
        tft.fillScreen(sClockBgColorActive);
        // tft.drawBitmap(0, 0, BmpRonds, 320, 240, TFT_WHITE);
        tft.drawBitmap(301, 4, image_clock_alarm_bits, 15, 16, TFT_WHITE); // Alarme en haut à droite

        sLastClock = "";
        sLastActiveCount = -1;
        lastSnoozeRemaining = -1;
        lastSnoozeVisible = false;
    }

    const bool baseChanged = force || clockText != sLastClock || activeCount != sLastActiveCount;
    if (baseChanged) {
        // Si ce n'est pas un redraw forcé, on redessine tout le fond car le texte a changé
        // (Obligatoire avec un fond en image BmpRonds pour "effacer" l'ancienne heure)
        if (!force) {
            tft.fillScreen(sClockBgColorActive);
            // tft.drawBitmap(0, 0, BmpRonds, 320, 240, TFT_WHITE);
            
            // On remet les icônes vu qu'on a repeint le fond
            tft.drawBitmap(301, 4, image_clock_alarm_bits, 15, 16, TFT_WHITE);
        }

        // --- AFFICHAGE DU NOMBRE D'ALARMES ---
        tft.setTextColor(TFT_WHITE, sClockBgColorActive);
        tft.setFreeFont(0);
        tft.setTextSize(2);
        tft.setTextDatum(TR_DATUM); // Alignement haut-droit
        // On place le chiffre juste à gauche de l'icône cloche (qui est à X=301)
        tft.drawString(String(activeCount), 295, 4); 

        // --- AFFICHAGE DE L'HEURE AU CENTRE ---
        tft.setTextColor(TFT_WHITE, sClockBgColorActive);
        tft.setFreeFont(&jgs_Font40pt7b);
        tft.setTextSize(1); // IMPORTANT : Toujours 1 avec les polices custom GFX
        tft.setTextDatum(MC_DATUM); // Alignement Milieu-Centre absolu
        tft.drawString(clockText, tft.width() / 2, tft.height() / 2 - 10);

        sLastClock = clockText;
        sLastActiveCount = activeCount;
    }

    drawWifiStatusIcon(force || baseChanged);

    // Gestion de l'overlay du Snooze (inchangée)
    if (snoozeVisible && snoozeRemaining > 0) {
        if (baseChanged || !lastSnoozeVisible || static_cast<int>(snoozeRemaining) != lastSnoozeRemaining) {
            drawSnoozeOverlay(baseChanged, snoozeRemaining);
            lastSnoozeRemaining = static_cast<int>(snoozeRemaining);
            lastSnoozeVisible = true;
        }
    } else if (lastSnoozeVisible) {
        drawSnoozeOverlay(true, 0);
        lastSnoozeRemaining = -1;
        lastSnoozeVisible = false;
    }
}

static void drawRingingScreen(bool force) {
    static String lastClock;
    static String lastLabel;

    String clockText = "--:--";
    String alarmLabel = alarmScheduler.currentRingingLabel();
    const time_t nowTs = time(nullptr);
    const struct tm* timeinfo = localtime(&nowTs);
    if (timeinfo && timeinfo->tm_year > 100) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
        clockText = String(buf);
    }

    if (force) {
        tft.fillScreen(TFT_BLACK);
        lastClock = "";
        lastLabel = "";
    }

    if (clockText != lastClock || alarmLabel != lastLabel) {
        const int w = tft.width();
        const int h = tft.height();
        const int cx = w / 2;

        tft.fillScreen(TFT_BLACK);

        // Conserver la police custom existante pour l'heure, mais centrée a l'ecran.
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setFreeFont(&jgs_Font40pt7b);
        tft.setTextSize(1);
        tft.drawString(clockText, cx - 6, h / 2 - 8);
        tft.setFreeFont(0);

        // Libelle alarme centré en haut.
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString(alarmLabel.length() > 0 ? alarmLabel : "Alarm", cx, 50, 2);

        // Boutons Stop/Snooze retires de l'ecran de reveil.

        lastClock = clockText;
        lastLabel = alarmLabel;
    }
}

static void drawWifiHelpScreen(bool force) {
    if (!force) {
        return;
    }

    tft.fillScreen(TFT_BLACK);

    tft.fillRoundRect(6, 6, tft.width() - 16, 42, 14, TFT_DARKCYAN);
    drawCentered("Se connecter au reseau", 8, 2, TFT_WHITE, TFT_DARKCYAN);
    drawCentered("Le portail s'ouvre", 25, 1, TFT_LIGHTGREY, TFT_DARKCYAN);
    drawCentered("automatiquement", 35, 1, TFT_LIGHTGREY, TFT_DARKCYAN);

    tft.drawFastHLine(12, 52, tft.width() - 24, TFT_DARKGREY);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("WiFi setup", 14, 60, 2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Scanne le QR code", 14, 78, 1);

    tft.drawString("SSID", 14, 94, 1);
    tft.drawString("Mot de passe", 14, 140, 1);
    tft.drawString("Adresse du portail", 14, 186, 1);

    tft.setTextDatum(TL_DATUM);
    drawInfoPill(14, 108, 150, 24, kApSsid, TFT_WHITE, TFT_BLACK, TFT_DARKGREY);
    drawInfoPill(14, 154, 150, 24, kApPass, TFT_WHITE, TFT_BLACK, TFT_DARKGREY);
    drawInfoPill(14, 200, 150, 24, kPortalUrl, TFT_CYAN, TFT_BLACK, TFT_DARKCYAN);

    const int qrScale = 4;
    const int qrSize = 33;
    const int qrPixelSize = qrSize * qrScale;
    const int qrX = tft.width() - qrPixelSize - 10;
    const int qrY = 66;
    // tft.fillRoundRect(qrX - 6, qrY - 6, qrPixelSize + 12, qrPixelSize + 12, 14, TFT_WHITE);
    drawQrCode("WIFI:T:WPA;S:PicoWake-Setup;P:picowake123;;", qrX, qrY, qrScale);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Scanne pour ouvrir", qrX + qrPixelSize / 2, qrY + qrPixelSize + 16, 1);
    tft.drawString("le portail", qrX + qrPixelSize / 2, qrY + qrPixelSize + 26, 1);
}

static void setup_wifi_first_connection() {
    if (kBypassWifiProvisioning) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println("[WIFI] bypass actif (WiFi OFF)");
        return;
    }

    wifiProvisioning.begin(kApSsid, kApPass);
    wifiProvisioning.connectStoredAsync(20000);

    if (!wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting()) {
        wifiProvisioning.startPortal();
    }
}

static UiScreen decideScreen() {
    if (alarmScheduler.isRinging()) return UiScreen::AlarmRinging;
    if (kBypassWifiProvisioning) return UiScreen::Loading;
    if (wifiProvisioning.isPortalActive()) return UiScreen::WifiHelp;
    if (wifiProvisioning.isConnected()) {
        if (sTimeInitialized) {
            if (millis() < sWeatherDisplayUntilMs) {
                return UiScreen::Weather;
            }
            return UiScreen::Clock;
        }
    }
    if (sTimeInitialized) return UiScreen::Clock;
    return UiScreen::Loading;
}

void setup() {
    hardware_init();
    Serial.printf("[BOOT] %lu ms | hardware_init termine\n", millis());

    alarmScheduler.begin();
    sLastSchedulerSettingsVersion = alarmScheduler.settingsVersion();

    drawLoadingScreen(true);

    setup_wifi_first_connection();
    applyBacklightImmediate(kBacklightDay);
    sLastWifiRetryMs = millis();
    Serial.printf("[BOOT] %lu ms | setup_wifi termine\n", millis());
}

void loop() {
    static uint32_t lastLoopMs = millis();
    static bool sLastAlarmRinging = false;
    const uint32_t nowMs = millis();
    const uint32_t loopDeltaMs = nowMs - lastLoopMs;
    lastLoopMs = nowMs;
    ++sLoopCount;

    if (loopDeltaMs > sMaxLoopDeltaMs) {
        sMaxLoopDeltaMs = loopDeltaMs;
    }

    hardware_update();
    const bool touch = hardware_touch_pressed();
    if (touch != sLastTouchState) {
        sLastTouchState = touch;
        if (touch) {
            if (alarmScheduler.isRinging()) {
                alarmScheduler.dismissRinging();
                sScreenDirty = true;
                
                // --- DECLENCHEMENT METEO ---
                const time_t nowTs = time(nullptr);
                const struct tm* ti = localtime(&nowTs);
                // Si l'alarme est stoppée entre 6h00 et 11h59
                if (ti && ti->tm_hour >= 6 && ti->tm_hour < 15) {
                    sWeatherDisplayUntilMs = millis() + (10UL * 60UL * 1000UL); // 10 minutes
                }
                // ---------------------------
                
            } else {
                hardware_buzzer_beep(30);
            }
        }
    }

   if (hardware_snooze_rising_edge()) {
        if (alarmScheduler.isRinging()) {
            // 1. Si une alarme sonne : on la reporte et on garde l'écran rose 10 secondes
            sSnoozePinkUntilMs = nowMs + kSnoozePinkDurationMs;
            alarmScheduler.snoozeRinging(10);
            applyBacklightImmediate(kBacklightSnooze);
            sScreenDirty = true;
        } else {
            // 2. Si aucune alarme ne sonne, on vérifie l'heure
            const time_t nowTs = time(nullptr);
            const struct tm* ti = localtime(&nowTs);
            
            // Si on est la nuit (écran éteint), on rallume temporairement l'écran
            if (sTimeInitialized && ti && isScreenOffSchedule(ti)) {
                sSnoozePinkUntilMs = nowMs + kSnoozePinkDurationMs;
                applyBacklightImmediate(kBacklightSnooze);
                sScreenDirty = true;
            }
            // Sinon (si c'est le jour), le code ne fait volontairement RIEN !
        }
    }

    if (!kBypassWifiProvisioning) {
        wifiProvisioning.loop();
        if (!wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting() && !wifiProvisioning.isPortalActive()) {
            if (sWifiRetryCount < kWifiRetryBeforePortal) {
                if ((nowMs - sLastWifiRetryMs) >= kWifiRetryIntervalMs) {
                    ++sWifiRetryCount;
                    sLastWifiRetryMs = nowMs;
                    Serial.printf("[WIFI] retry %u/%u before portal\n",
                                  static_cast<unsigned>(sWifiRetryCount),
                                  static_cast<unsigned>(kWifiRetryBeforePortal));
                    wifiProvisioning.connectStoredAsync(20000);
                }
            } else {
                wifiProvisioning.startPortal();
            }
        } else if (wifiProvisioning.isConnected()) {
            sWifiRetryCount = 0;
        }
    }

    if (!kBypassWifiProvisioning && wifiProvisioning.isConnected()) {
        if (!sNtpStarted && (nowMs - sLastNtpAttemptMs >= kNtpRetryIntervalMs || sLastNtpAttemptMs == 0)) {
            sLastNtpAttemptMs = nowMs;
            IPAddress ntpServer;
            if (WiFi.hostByName("pool.ntp.org", ntpServer) == 1) {
                NTP.begin(ntpServer, 3600);
                setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
                tzset();
                sNtpStarted = true;
                Serial.printf("[NTP] started with server %s\n", ntpServer.toString().c_str());
            } else {
                Serial.println("[NTP] DNS resolve failed, will retry");
            }
        }

        const bool prevTimeInit = sTimeInitialized;
        sTimeInitialized = systemTimeIsValid();
        if (!prevTimeInit && sTimeInitialized) {
            Serial.println("[NTP] time synchronized");
            sScreenDirty = true;
        }
    } else {
        sNtpStarted = false;
    }

    if (!kBypassWifiProvisioning && wifiProvisioning.isConnected()) {
        alarmScheduler.ensureWebStarted("picowake");
    } else {
        alarmScheduler.stopWeb();
    }
    alarmScheduler.loop();

    refreshSunTimesIfNeeded(nowMs);

    const bool ringingNow = alarmScheduler.isRinging();
    if (ringingNow && !sLastAlarmRinging) {
        applyBacklightImmediate(kBacklightDay);
        sScreenDirty = true;
    }
    sLastAlarmRinging = ringingNow;

    const uint32_t schedulerSettingsVersion = alarmScheduler.settingsVersion();
    if (schedulerSettingsVersion != sLastSchedulerSettingsVersion) {
        sLastSchedulerSettingsVersion = schedulerSettingsVersion;
        sScreenDirty = true;
    }

    if (updateClockTheme()) {
        sScreenDirty = true;
    }

    if (ringingNow) {
        setBacklightTarget(kBacklightDay);
    }
    serviceBacklightFade(nowMs);

    const UiScreen nextScreen = decideScreen();
    if (nextScreen != sCurrentScreen) {
        sCurrentScreen = nextScreen;
        sScreenDirty = true;
        if (sCurrentScreen != UiScreen::WifiHelp) {
            sWifiHelpPortalPrimed = false;
        }
    }

    if (sCurrentScreen == UiScreen::WifiHelp) {
        if (!sWifiHelpPortalPrimed) {
            if (!wifiProvisioning.isPortalActive() && !wifiProvisioning.isConnecting()) {
                wifiProvisioning.startPortal();
            }
            sWifiHelpPortalPrimed = true;
            sScreenDirty = true;
        }
    }

    if (sCurrentScreen == UiScreen::AlarmRinging) {
        drawRingingScreen(sScreenDirty);
    } else if (sCurrentScreen == UiScreen::Loading) {
        drawLoadingScreen(sScreenDirty);
    } else if (sCurrentScreen == UiScreen::Weather) { 
        drawWeatherScreen(sScreenDirty);
    } else if (sCurrentScreen == UiScreen::Clock) {
        String clockText = "--:--";
        const time_t now = time(nullptr);
        const struct tm* timeinfo = localtime(&now);
        if (timeinfo && timeinfo->tm_year > 100) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
            clockText = String(buf);
        }
        drawClockScreen(sScreenDirty, clockText);
    } else {
        drawWifiHelpScreen(sScreenDirty);
    }

    sScreenDirty = false;

    static uint32_t lastDebugMs = 0;
    if ((nowMs - lastDebugMs) > 1000) {
        lastDebugMs = nowMs;
        Serial.printf(
            "[DBG] uptime=%lu ms | loops=%lu | maxDelta=%lu ms | touch=%d | wifiBypass=%d | screen=%d\n",
            nowMs,
            static_cast<unsigned long>(sLoopCount),
            static_cast<unsigned long>(sMaxLoopDeltaMs),
            touch ? 1 : 0,
            kBypassWifiProvisioning ? 1 : 0,
            static_cast<int>(sCurrentScreen)
        );
        sMaxLoopDeltaMs = 0;
    }

    delay(5);
}