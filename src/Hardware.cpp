#include "Hardware.h"
#include <SPI.h>

#if __has_include(<hardware/watchdog.h>)
#include <hardware/watchdog.h>
#define PICO_HAS_WATCHDOG 1
#else
#define PICO_HAS_WATCHDOG 0
#endif

TFT_eSPI tft = TFT_eSPI(); 

static bool sTouchPressed = false;
static bool sTouchPrev = false;
static bool sTouchRisingEdge = false;
static bool sSnoozePressed = false;
static bool sSnoozePrev = false;
static bool sSnoozeRisingEdge = false;

void hardware_buzzer_on() {
    digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
}

void hardware_buzzer_off() {
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
}

void hardware_buzzer_beep(uint16_t durationMs) {
    hardware_buzzer_on();
    delay(durationMs);
    hardware_buzzer_off();
}

bool hardware_touch_pressed() {
    return sTouchPressed;
}

bool hardware_touch_rising_edge() {
    const bool edge = sTouchRisingEdge;
    sTouchRisingEdge = false;
    return edge;
}

bool hardware_snooze_pressed() {
    return sSnoozePressed;
}

bool hardware_snooze_rising_edge() {
    const bool edge = sSnoozeRisingEdge;
    sSnoozeRisingEdge = false;
    return edge;
}

void hardware_update() {
    sTouchPressed = (digitalRead(PIN_TOUCH_TTP223) == HIGH);
    sTouchRisingEdge = (!sTouchPrev && sTouchPressed);
    sTouchPrev = sTouchPressed;

    sSnoozePressed = (digitalRead(PIN_SNOOZE_BUTTON) == LOW);
    sSnoozeRisingEdge = (!sSnoozePrev && sSnoozePressed);
    sSnoozePrev = sSnoozePressed;
}

void hardware_init() {
    // 1. ALLUMAGE IMMÉDIAT DU RÉTROÉCLAIRAGE
    // On le fait en tout premier pour voir quelque chose même si ça plante après
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    // 1b. Entrées/sorties dédiées au nouveau hardware.
    pinMode(PIN_BUZZER_ACTIVE, OUTPUT);
    hardware_buzzer_off();
    pinMode(PIN_TOUCH_TTP223, INPUT);
    pinMode(PIN_SNOOZE_BUTTON, INPUT_PULLUP);

    Serial.begin(115200);

    #if PICO_HAS_WATCHDOG
    const uint32_t kWdMagic = 0x50434F48u;
    const bool wd = watchdog_caused_reboot() || (watchdog_hw->scratch[0] == kWdMagic);
    if (wd) {
        const unsigned long start = millis();
        while (!Serial && (millis() - start < 2000)) delay(10);
    }
    delay(50);
    watchdog_hw->scratch[0] = 0;
    #endif

    // 2. PRÉPARATION DES PINS SPI & CS (Anti-Conflit)
    // On désactive explicitement le Touch et la SD avant de toucher à l'écran
    pinMode(TP_CS, OUTPUT); digitalWrite(TP_CS, HIGH);
    pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH); // SD_CS est GP22

    // 3. INIT ECRAN
    // L'écran utilise SPI1 (GP10, 11, 12) défini dans User_Setup.h de TFT_eSPI
    tft.init();
    tft.setRotation(3); 
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SYSTEM BOOT...", 5, 5, 2);

    // 4. INIT FILESYSTEM (Interne)
    if(!LittleFS.begin()) {
        tft.drawString("Formatting FS...", 5, 25, 2);
        LittleFS.format();
        LittleFS.begin();
    }

    // Snapshot initial de l'etat tactile.
    hardware_update();
}