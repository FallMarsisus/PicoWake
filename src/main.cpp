#include <Arduino.h>
#include "Hardware.h"
#include "FirstWifiProvisioning.h"
#include <WiFiNTP.h>
#include <time.h>
#include <qrcode.h>
#include "assets/ronds.h"
#include "AlarmScheduler.h"

static constexpr bool kBypassWifiProvisioning = false;
static constexpr const char* kApSsid = "PicoWake-Setup";
static constexpr const char* kApPass = "picowake123";
static constexpr const char* kPortalUrl = "http://192.168.4.1";
// "ffo62" interprete comme "ff062" (o -> 0), soit RGB(15,240,98)
static constexpr uint16_t kClockBgColor = 55499;

static FirstWifiProvisioning wifiProvisioning;
static AlarmScheduler alarmScheduler;
static bool sLastTouchState = false;
static bool sTimeInitialized = false;
static bool sNtpStarted = false;

enum class UiScreen {
    Loading,
    Clock,
    WifiHelp,
    AlarmRinging,
};

static UiScreen sCurrentScreen = UiScreen::Loading;
static bool sScreenDirty = true;
static String sLastClock;
static int sLastActiveCount = -1;
static uint32_t sLastLoadingAnimMs = 0;
static uint8_t sLoadingFrame = 0;

static uint32_t sLoopCount = 0;
static uint32_t sMaxLoopDeltaMs = 0;

static bool systemTimeIsValid() {
    const time_t nowTs = time(nullptr);
    const struct tm* timeinfo = localtime(&nowTs);
    return timeinfo && timeinfo->tm_year > 100;
}

static void drawCentered(const String& text, int y, int font, uint16_t fg, uint16_t bg = TFT_BLACK) {
    tft.setTextColor(fg, bg);
    tft.drawCentreString(text, tft.width() / 2, y, font);
}

static void drawQrCode(const char* payload) {
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    QRCode qrcode;
    qrcode_initText(&qrcode, qrcodeData, 4, ECC_MEDIUM, payload);

    const int size = qrcode.size;
    const int scale = 4;
    const int qrPixelSize = size * scale;
    const int x0 = (tft.width() - qrPixelSize) / 2;
    const int y0 = tft.height() - qrPixelSize - 8;

    tft.fillRect(x0 - 4, y0 - 4, qrPixelSize + 8, qrPixelSize + 8, TFT_WHITE);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qrcode_getModule(&qrcode, x, y)) {
                tft.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, TFT_BLACK);
            }
        }
    }
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
            tft.fillRoundRect(lastOverlayX - 2, lastOverlayY - 2, lastOverlayW + 4, lastOverlayH + 4, 12, kClockBgColor);
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
        tft.fillRoundRect(lastOverlayX - 2, lastOverlayY - 2, lastOverlayW + 4, lastOverlayH + 4, 12, kClockBgColor);
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

static void drawClockScreen(bool force, const String& clockText) {
    const int activeCount = static_cast<int>(alarmScheduler.activeEnabledCount());
    const bool snoozeVisible = alarmScheduler.hasSnooze();
    const uint32_t snoozeRemaining = snoozeVisible ? alarmScheduler.snoozeRemainingSeconds() : 0;
    static int lastSnoozeRemaining = -1;
    static bool lastSnoozeVisible = false;

    if (force) {
        tft.fillScreen(kClockBgColor);
        tft.drawBitmap(0, 0, BmpRonds, 320, 240, TFT_WHITE);
        sLastClock = "";
        sLastActiveCount = -1;
        lastSnoozeRemaining = -1;
        lastSnoozeVisible = false;
    }

    const bool baseChanged = force || clockText != sLastClock || activeCount != sLastActiveCount;
    if (baseChanged) {
        tft.fillScreen(kClockBgColor);
        tft.drawBitmap(0, 0, BmpRonds, 320, 240, TFT_WHITE);

        tft.drawBitmap(8, 8, alarm, 24, 24, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, kClockBgColor);
        tft.setTextDatum(TL_DATUM);
        tft.setTextSize(2);
        tft.drawString(String(activeCount), 40, 4, 2);
        tft.setTextSize(1);

        tft.setTextColor(TFT_WHITE, kClockBgColor);
        tft.setTextDatum(TR_DATUM);
        tft.setTextSize(2);
        tft.drawString(clockText, tft.width() - 8, 8, 4);
        tft.setTextSize(1);

        sLastClock = clockText;
        sLastActiveCount = activeCount;
    }

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

        if (alarmLabel.length() > 0) {
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.setTextSize(1);
            tft.drawString(alarmLabel, cx, h / 2 - 72, 2);
        }

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(3);
        tft.drawString(clockText, cx, h / 2 - 34 + 25, 4);
        tft.setTextSize(1);

        const int btnW = 156;
        const int btnH = 44;
        const int btnX = (w - btnW) / 2;
        const int btnY = h / 2 + 34;

        // Bouton arrondi simple blanc
        tft.fillRoundRect(btnX, btnY, btnW, btnH, 12, TFT_WHITE);
        tft.drawRoundRect(btnX, btnY, btnW, btnH, 12, TFT_LIGHTGREY);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.drawString("Arreter", cx, btnY + btnH / 2 + 3, 4);

        lastClock = clockText;
        lastLabel = alarmLabel;
    }
}

static void drawWifiHelpScreen(bool force) {
    if (!force) {
        return;
    }

    tft.fillScreen(TFT_BLACK);
    drawCentered("Echec connexion WiFi", 8, 2, TFT_RED);
    tft.drawFastHLine(0, 28, tft.width(), TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("1) Connecte-toi au reseau:", 8, 36, 2);
    tft.drawString(kApSsid, 8, 54, 4);
    tft.drawString("2) Mot de passe:", 8, 84, 2);
    tft.drawString(kApPass, 8, 102, 2);
    tft.drawString("3) Ouvre:", 8, 120, 2);
    tft.drawString(kPortalUrl, 8, 138, 2);

    drawQrCode("WIFI:T:WPA;S:PicoWake-Setup;P:picowake123;;");
}

static void setup_wifi_first_connection() {
    if (kBypassWifiProvisioning) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println("[WIFI] bypass actif (WiFi OFF)");
        return;
    }

    wifiProvisioning.begin(kApSsid, kApPass);
    wifiProvisioning.connectStoredAsync(12000);

    if (!wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting()) {
        wifiProvisioning.startPortal();
    }
}

static UiScreen decideScreen() {
    if (alarmScheduler.isRinging()) {
        return UiScreen::AlarmRinging;
    }
    if (kBypassWifiProvisioning) {
        return UiScreen::Loading;
    }
    if (wifiProvisioning.isPortalActive()) {
        return UiScreen::WifiHelp;
    }
    if (!sTimeInitialized) {
        return UiScreen::Loading;
    }
    if (wifiProvisioning.isConnected()) {
        return UiScreen::Clock;
    }
    return UiScreen::Clock;
}

void setup() {
    hardware_init();
    Serial.printf("[BOOT] %lu ms | hardware_init termine\n", millis());

    alarmScheduler.begin();

    drawLoadingScreen(true);

    setup_wifi_first_connection();
    Serial.printf("[BOOT] %lu ms | setup_wifi termine\n", millis());
}

void loop() {
    static uint32_t lastLoopMs = millis();
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
            } else {
                hardware_buzzer_beep(30);
            }
        }
    }

    if (hardware_snooze_rising_edge()) {
        if (alarmScheduler.isRinging()) {
            alarmScheduler.snoozeRinging(10);
            sScreenDirty = true;
        }
    }

    if (!kBypassWifiProvisioning) {
        wifiProvisioning.loop();
        if (!wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting() && !wifiProvisioning.isPortalActive()) {
            wifiProvisioning.startPortal();
        }
    }

    if (!kBypassWifiProvisioning && wifiProvisioning.isConnected()) {
        if (!sNtpStarted) {
            NTP.begin("pool.ntp.org", 3600);
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
            tzset();
            sNtpStarted = true;
        }
        if (!sTimeInitialized) {
            sTimeInitialized = systemTimeIsValid();
            if (sTimeInitialized) {
                sScreenDirty = true;
            }
        }
    } else if (!sTimeInitialized) {
        sNtpStarted = false;
    }

    if (!kBypassWifiProvisioning && wifiProvisioning.isConnected()) {
        alarmScheduler.ensureWebStarted("picowake");
    } else {
        alarmScheduler.stopWeb();
    }
    alarmScheduler.loop();

    const UiScreen nextScreen = decideScreen();
    if (nextScreen != sCurrentScreen) {
        sCurrentScreen = nextScreen;
        sScreenDirty = true;
    }

    if (sCurrentScreen == UiScreen::AlarmRinging) {
        drawRingingScreen(sScreenDirty);
    } else if (sCurrentScreen == UiScreen::Loading) {
        drawLoadingScreen(sScreenDirty);
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