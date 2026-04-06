#include <Arduino.h>
#include "Hardware.h"
#include <lvgl.h>
#include "FirstWifiProvisioning.h"
#include <WiFiNTP.h>
#include <time.h> // Pour le NTP (Heure internet)

static constexpr bool kBypassWifiProvisioning = true;
static constexpr uint32_t kLoopStallWarningMs = 80;
static constexpr bool kProbeDelayLvglInLoop = false;
static constexpr uint32_t kProbeDelayLvglMs = 5000;
static constexpr bool kBypassTftFlushForDebug = false;

static lv_disp_draw_buf_t sDrawBuf;
static lv_color_t sBuf1[240 * 20];

static lv_obj_t* sLabelTouch = nullptr;
static lv_obj_t* sLabelHint = nullptr;
static lv_obj_t* sLabelWifi = nullptr;
static lv_obj_t* sLabelTime = nullptr; // Nouvelle étiquette pour l'heure

static bool sLastTouchState = false;
static bool sTimeInitialized = false;
static FirstWifiProvisioning wifiProvisioning;
static uint32_t sLoopCount = 0;
static uint32_t sMaxLoopDeltaMs = 0;
static uint32_t sFlushSeq = 0;
static uint32_t sLvLoopCalls = 0;
static bool sTftDmaReady = false;
static bool sLoopStartedLogged = false;
static bool sLoopLvglEnabledLogged = false;

static void debug_log_setup(const char* step) {
    Serial.printf("[BOOT] %lu ms | %s\n", millis(), step);
}

static void ui_set_wifi_label(const char* text, lv_color_t color) {
    if (!sLabelWifi) return;
    lv_label_set_text(sLabelWifi, text);
    lv_obj_set_style_text_color(sLabelWifi, color, 0);
}

static void setup_wifi_first_connection() {
    if (kBypassWifiProvisioning) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        ui_set_wifi_label("WiFi: bypass debug actif", lv_color_hex(0xFFB38A));
        Serial.println("[WIFI] Bypass actif: provisioning et AP desactives pour debug freeze");
        return;
    }

    wifiProvisioning.begin("PicoWake-Setup", "picowake123");
    ui_set_wifi_label("WiFi: tentative connexion...", lv_color_hex(0xF4DFA5));
    wifiProvisioning.connectStoredAsync(12000);

    if (!wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting()) {
        wifiProvisioning.startPortal();
        ui_set_wifi_label("WiFi: portail de config actif", lv_color_hex(0xFFB38A));
    }
}

static void lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    const uint32_t flushId = ++sFlushSeq;
    const uint32_t flushStartUs = micros();
    if (flushId <= 8) {
        Serial.printf(
            "[LVGL] flush #%lu ENTER area=(%d,%d)-(%d,%d)\n",
            static_cast<unsigned long>(flushId),
            area->x1,
            area->y1,
            area->x2,
            area->y2
        );
    }

    if (kBypassTftFlushForDebug) {
        if (flushId <= 8) {
            Serial.printf("[LVGL] flush #%lu BYPASS (no TFT write)\n", static_cast<unsigned long>(flushId));
        }
        lv_disp_flush_ready(disp);
        return;
    }

    const uint16_t w = static_cast<uint16_t>(area->x2 - area->x1 + 1);
    const uint16_t h = static_cast<uint16_t>(area->y2 - area->y1 + 1);

    if (flushId <= 8) {
        Serial.printf("[LVGL] flush #%lu stage startWrite\n", static_cast<unsigned long>(flushId));
    }

    tft.startWrite();
    if (flushId <= 8) {
        Serial.printf("[LVGL] flush #%lu stage setAddrWindow\n", static_cast<unsigned long>(flushId));
    }
    tft.setAddrWindow(area->x1, area->y1, w, h);

    if (sTftDmaReady) {
        if (flushId <= 8) {
            Serial.printf("[LVGL] flush #%lu stage pushPixelsDMA\n", static_cast<unsigned long>(flushId));
        }
        tft.pushPixelsDMA(reinterpret_cast<uint16_t*>(color_p), static_cast<uint32_t>(w) * h);
        if (flushId <= 8) {
            Serial.printf("[LVGL] flush #%lu stage dmaWait\n", static_cast<unsigned long>(flushId));
        }
        tft.dmaWait();
    } else {
        if (flushId <= 8) {
            Serial.printf("[LVGL] flush #%lu stage pushColors\n", static_cast<unsigned long>(flushId));
        }
        tft.pushColors(reinterpret_cast<uint16_t*>(color_p), static_cast<uint32_t>(w) * h, true);
    }

    if (flushId <= 8) {
        Serial.printf("[LVGL] flush #%lu stage endWrite\n", static_cast<unsigned long>(flushId));
    }
    tft.endWrite();

    if (flushId <= 8) {
        Serial.printf(
            "[LVGL] flush #%lu EXIT in %lu us\n",
            static_cast<unsigned long>(flushId),
            static_cast<unsigned long>(micros() - flushStartUs)
        );
    }

    lv_disp_flush_ready(disp);
}

static void init_lvgl_ui() {
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0E1A2B), 0);
    lv_obj_set_style_bg_grad_color(lv_scr_act(), lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_bg_grad_dir(lv_scr_act(), LV_GRAD_DIR_VER, 0);

    lv_obj_t* title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "PicoWake");
    lv_obj_set_style_text_color(title, lv_color_hex(0xEAF2FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    sLabelTouch = lv_label_create(lv_scr_act());
    lv_label_set_text(sLabelTouch, "Touch: relache");
    lv_obj_set_style_text_color(sLabelTouch, lv_color_hex(0xB5D7FF), 0);
    lv_obj_set_style_text_font(sLabelTouch, &lv_font_montserrat_14, 0);
    lv_obj_align(sLabelTouch, LV_ALIGN_CENTER, 0, -20);

    sLabelHint = lv_label_create(lv_scr_act());
    lv_label_set_text(sLabelHint, "Tap capteur GP0");
    lv_obj_set_style_text_color(sLabelHint, lv_color_hex(0x8FAED0), 0);
    lv_obj_set_style_text_font(sLabelHint, &lv_font_montserrat_14, 0);
    lv_obj_align(sLabelHint, LV_ALIGN_CENTER, 0, 10);

    // Initialisation de l'étiquette pour l'heure (masquée par défaut)
    sLabelTime = lv_label_create(lv_scr_act());
    lv_label_set_text(sLabelTime, "");
    lv_obj_set_style_text_color(sLabelTime, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(sLabelTime, &lv_font_montserrat_48, 0);
    lv_obj_align(sLabelTime, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sLabelTime, LV_OBJ_FLAG_HIDDEN);

    sLabelWifi = lv_label_create(lv_scr_act());
    lv_label_set_text(sLabelWifi, "WiFi: non initialise");
    lv_obj_set_style_text_color(sLabelWifi, lv_color_hex(0xF4DFA5), 0);
    lv_obj_set_style_text_font(sLabelWifi, &lv_font_montserrat_14, 0);
    lv_obj_align(sLabelWifi, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void setup() {
    hardware_init();
    debug_log_setup("hardware_init termine");

    sTftDmaReady = tft.initDMA();
    Serial.printf("[BOOT] %lu ms | tft.initDMA=%d\n", millis(), sTftDmaReady ? 1 : 0);

    // Suppression du hardware_self_test();

    lv_init();
    debug_log_setup("lv_init termine");
    lv_disp_draw_buf_init(&sDrawBuf, sBuf1, nullptr, sizeof(sBuf1) / sizeof(sBuf1[0]));
    debug_log_setup("draw buffer LVGL initialise");

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = tft.width();
    disp_drv.ver_res = tft.height();
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &sDrawBuf;
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    lv_disp_set_default(disp);
    debug_log_setup("driver display LVGL enregistre");

    init_lvgl_ui();
    debug_log_setup("UI LVGL initialisee");
    lv_timer_handler();
    debug_log_setup("premier lv_timer_handler execute");
    setup_wifi_first_connection();
    debug_log_setup("setup_wifi_first_connection termine");
}

void loop() {
    static uint32_t lastTickMs = millis();
    const uint32_t now = millis();
    const uint32_t loopDeltaMs = now - lastTickMs;
    lv_tick_inc(loopDeltaMs);
    lastTickMs = now;
    ++sLoopCount;

    if (!sLoopStartedLogged) {
        sLoopStartedLogged = true;
        Serial.printf("[BOOT] %lu ms | loop started\n", millis());
    }

    if (loopDeltaMs > sMaxLoopDeltaMs) {
        sMaxLoopDeltaMs = loopDeltaMs;
    }
    if (loopDeltaMs > kLoopStallWarningMs) {
        Serial.printf("[WARN] Stall loop detecte: %lu ms (seuil %lu ms)\n", loopDeltaMs, static_cast<unsigned long>(kLoopStallWarningMs));
    }

    hardware_update();
    if (!kBypassWifiProvisioning) {
        wifiProvisioning.loop();
    }

    if (!kBypassWifiProvisioning && !wifiProvisioning.isConnected() && !wifiProvisioning.isConnecting() && !wifiProvisioning.isPortalActive()) {
        wifiProvisioning.startPortal();
        ui_set_wifi_label("WiFi: portail de config actif", lv_color_hex(0xFFB38A));
    }

    // --- GESTION DU WIFI ET DE L'HEURE ---
    if (!kBypassWifiProvisioning && wifiProvisioning.isConnected()) {
        // Initialiser l'heure internet (NTP) une seule fois
        if (!sTimeInitialized) {
            NTP.begin("pool.ntp.org", 3600);
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Fuseau horaire Paris
            tzset();
            sTimeInitialized = true;
            
            // Masquer les labels de dev et afficher l'heure
            lv_obj_add_flag(sLabelTouch, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(sLabelHint, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(sLabelTime, LV_OBJ_FLAG_HIDDEN);
        }

        // Mettre à jour l'affichage de l'heure chaque seconde
        static uint32_t lastTimeUpdate = 0;
        if (millis() - lastTimeUpdate > 1000) {
            lastTimeUpdate = millis();
            time_t now = time(nullptr);
            struct tm* timeinfo = localtime(&now);
            
            if (timeinfo->tm_year > 100) { // Si l'année est > 2000, l'heure est synchronisée
                char timeStr[16];
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
                lv_label_set_text(sLabelTime, timeStr);
            } else {
                lv_label_set_text(sLabelTime, "--:--");
            }
        }
    }

    static uint32_t wifiLabelRefreshMs = 0;
    if (!kBypassWifiProvisioning && (millis() - wifiLabelRefreshMs) > 1000) {
        wifiLabelRefreshMs = millis();
        const String wifiStatus = wifiProvisioning.statusMessage();
        if (wifiStatus.length() > 0) {
            ui_set_wifi_label(wifiStatus.c_str(), wifiProvisioning.isConnected() ? lv_color_hex(0x92F2B3) : lv_color_hex(0xF4DFA5));
        }
    }

    const bool touch = hardware_touch_pressed();

    if (touch != sLastTouchState) {
        sLastTouchState = touch;
        if (touch) {
            if(!sTimeInitialized) lv_label_set_text(sLabelTouch, "Touch: appuye");
            hardware_buzzer_beep(30);
        } else {
            if(!sTimeInitialized) lv_label_set_text(sLabelTouch, "Touch: relache");
        }
    }

    uint32_t lvDurationUs = 0;
    const bool lvglEnabledInLoop = !kProbeDelayLvglInLoop || (millis() >= kProbeDelayLvglMs);
    if (lvglEnabledInLoop) {
        if (!sLoopLvglEnabledLogged) {
            sLoopLvglEnabledLogged = true;
            Serial.printf("[LVGL] loop handler active at %lu ms\n", millis());
        }
        ++sLvLoopCalls;
        if (sLvLoopCalls <= 8) {
            Serial.printf("[LVGL] handler #%lu ENTER\n", static_cast<unsigned long>(sLvLoopCalls));
        }
        const uint32_t lvStartUs = micros();
        lv_timer_handler();
        lvDurationUs = micros() - lvStartUs;
        if (sLvLoopCalls <= 8) {
            Serial.printf(
                "[LVGL] handler #%lu EXIT in %lu us\n",
                static_cast<unsigned long>(sLvLoopCalls),
                static_cast<unsigned long>(lvDurationUs)
            );
        }
    }

    static uint32_t lastDebugMs = 0;
    if ((millis() - lastDebugMs) > 1000) {
        lastDebugMs = millis();
        Serial.printf(
            "[DBG] uptime=%lu ms | loops=%lu | maxDelta=%lu ms | lv=%lu us | lvLoop=%d | flushes=%lu | touch=%d | wifiBypass=%d\n",
            millis(),
            static_cast<unsigned long>(sLoopCount),
            static_cast<unsigned long>(sMaxLoopDeltaMs),
            static_cast<unsigned long>(lvDurationUs),
            lvglEnabledInLoop ? 1 : 0,
            static_cast<unsigned long>(sFlushSeq),
            hardware_touch_pressed() ? 1 : 0,
            kBypassWifiProvisioning ? 1 : 0
        );
        sMaxLoopDeltaMs = 0;
    }

    delay(5);
}