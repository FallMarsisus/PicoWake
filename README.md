# PicoWake (Tomat'O-Clock) 🍅⏰

Meet **Tomat'O-Clock**, a smart, connected, tomato-shaped alarm clock! 

This repository contains the source code and documentation for the PicoWake project. Built around a Raspberry Pi Pico 2 W and a touch screen, this project provides a fully configurable alarm clock with a Web UI, WiFi setup portal, and NTP time synchronization. 

The physical build features a capacitive touch sensor for dismissing alarms and a mechanical keyboard switch hidden in the tomato's stem acting as the "Snooze" button.

## ✨ Features
* **Captive WiFi Portal:** Easy network setup on first boot.
* **NTP Time Sync:** Always keeps accurate time over the internet.
* **Local Web UI:** Set multiple alarms, adjust volume, and manage days directly from your browser.
* **Touch Screen Interface:** Interactive local display.
* **Custom Hardware Controls:** Capacitive touch to dismiss, mechanical switch to snooze.

---

## 🚀 Software Setup & Flashing

The source code is written using the **Arduino framework** via **PlatformIO**.

### Prerequisites
* [VSCode](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/) installed.
* Git

### Building and Flashing
1. Clone this repository:
   ```bash
   git clone https://github.com/FallMarsisus/PicoWake.git
   ```
2. Open the `PicoWake` project folder in VSCode. PlatformIO will automatically initialize the environment and download dependencies.
3. Plug your Raspberry Pi Pico 2 W into your computer via USB.
4. Click the **"Upload"** arrow in the PlatformIO bottom toolbar to compile and flash the firmware.
5. Once flashed, the Pico will restart. On the first boot, it will create a temporary WiFi Access Point. Connect to it with your phone or computer to configure your local WiFi credentials.

---

## 🔌 Pinout & Wiring

The Pico is designed to be embedded directly into the back of the Waveshare screen. Solder the peripherals directly to the Pico's pins or the screen's exposed pads.

| Component | Pico Pin | Notes |
| :--- | :--- | :--- |
| **TTP223 Touch Sensor** (Dismiss) | `GP0` | Signal pin |
| **Keyboard Switch** (Snooze) | `GP1`, `GND` | Code uses internal `INPUT_PULLUP` |
| **Passive Buzzer** | `GP14` | Positive/Signal pin |
| **USB-C Port** | `VBUS`, `GND` | Power delivery |

---

## 🛠️ Hardware & Bill of Materials (BOM)

*(Note: For 3D printing files and physical assembly guides, please refer to the project's MakerWorld page).*

* **Microcontroller:** [Raspberry Pi Pico 2 W](https://www.raspberrypi.com/products/raspberry-pi-pico-2-w/) (A classic Pico W should also work).
* **Display:** Waveshare Pico ResTouch LCD 2.8". The Pico plugs directly into its back.
* **Touch Sensor:** TTP223 Capacitive Touch Module.
* **Snooze Button:** Any mechanical keyboard switch.
* **Sound:** Standard passive buzzer.
* **Power:** 4-pin Type-C USB breakout board.
* **Hardware (Optional):** M3 threaded inserts and M3 screws (for the backplate).
* **Miscellaneous:** Flexible wires, soldering iron, and glue.

## 🏗️ Physical Assembly Overview

1. **Prepare the Screen:** Plug the Pico 2 W into the back of the Waveshare screen.
2. **Solder Components:** Cut wires to generous lengths and solder the touch sensor, buzzer, and USB-C port to the Pico.
3. **Mount the USB-C Port:** Secure it at the back of the shell, aligned with the backplate.
4. **Mount the Stem (Snooze) ⚠️:** Route the wires through the top hole of the shell *before* soldering them to the keyboard switch. Once soldered, click the switch into its housing.
5. **Hide Touch Sensor:** Mount the TTP223 sensor inside the shell, behind the front face.
6. **Close Up:** You can leave the back open for easy access or seal it using the backplate, M3 inserts, and screws.
