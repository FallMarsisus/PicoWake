#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h> 

// --- PINS PRINCIPAUX ---
constexpr uint8_t PIN_TOUCH_TTP223 = 0; // GP0
constexpr uint8_t PIN_SNOOZE_BUTTON = 1; // GP1
constexpr uint8_t PIN_BUZZER_ACTIVE = 14; // GP14
constexpr uint8_t PIN_TFT_BL = 13; // GP13

// --- PINS A NEUTRALISER ---
#define TP_CS 16 
#define SD_CS 22 

#define PIN_SD_CS   22

// Désactive l'initialisation SD au boot si le câblage SD/SDIO est instable.
// Override via build_flags: -D PICO_SDIO_ENABLE=0
#ifndef PICO_SDIO_ENABLE
#define PICO_SDIO_ENABLE 1
#endif

// Objet écran global (Déclaration uniquement)
extern TFT_eSPI tft;

// Initialisation globale du matériel.
void hardware_init();


// Mise à jour périodique du module Hardware.
void hardware_update();

// Helpers buzzer.
void hardware_buzzer_on();
void hardware_buzzer_off();
void hardware_buzzer_beep(uint16_t durationMs = 60);

// Etat courant du capteur tactile TTP223.
bool hardware_touch_pressed();
bool hardware_touch_rising_edge();
bool hardware_snooze_pressed();
bool hardware_snooze_rising_edge();

#endif