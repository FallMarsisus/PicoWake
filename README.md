# PicoWake (Meet Tomat'O-Clock)
Meet Tomat'O-Clock, a cute and smart tomato-shaped connected alarm clock! This project uses a Raspberry Pi Pico 2 W, a touch screen, and a few simple electronic components to create a fully configurable alarm clock.

The tomato's stem uses an actual mechanical keyboard switch acting like a "Snooze" button.

## Bill of Materials
* **Microcontroller:** Raspberry Pi Pico 2 W (A classic Pico W should also work, I didn't test it though).
* **Display:** Waveshare Pico ResTouch LCD 2.8" (Waveshare Link). The Pico plugs directly into its back, once you're finished building, it goes straight on the front.
* **Touch Sensor:** TTP223 Capacitive Touch Module (AliExpress Link). This will be hidden behind the front bezel to easily dismiss the alarm with a tap, you'll notice the hole in the structure to insert it.
* **Snooze Button:** Any mechanical keyboard switch of your choice.
* **Sound:** A standard passive buzzer.
* **Power:** A basic 4-pin Type-C USB breakout board (AliExpress Link). (Note: This could be upgraded to a module that supports USB-C to USB-C charging in the future).
* **Hardware (Optional):** M3 threaded inserts and M3 screws (only if you want to use the optional backplate).
* **Miscellaneous:** Flexible wires (make sure they are long enough!), soldering iron, and some glue.

## Wiring
The Pico is designed to be embedded directly into the back of the Waveshare screen. You will need to solder your wires directly to the Pico's pins (or the screen's exposed pads) to connect the peripherals. All wiring will be hidden inside the tomato shell.

* **TTP223 Touch Sensor (Dismiss):** Connect the signal pin to GP0.
* **Keyboard Switch (Snooze):** Connect one pin to GP1 and the other to GND (the code uses the internal INPUT_PULLUP resistor).
* **Passive Buzzer:** Connect the positive/signal pin to GP14.
* **USB-C Port:** Connect the VBUS/VCC/+ from the USB-C board to the VBUS on the Pico, and GND/- to GND.

## Building
1. **Prepare the Screen:** Plug the Raspberry Pi Pico 2 W into the back of the Waveshare screen.
2. **Solder the Components:** Cut your wires to a generous length. Solder the touch sensor, the buzzer, and the power wires directly to the corresponding pins on the Pico.
3. **Mount the USB-C Port:** Simply glue it at the back of the shell, aligned with the backplate.
4. **Mount the Stem (Snooze) - ⚠️ Crucial Step:** First, route the wires meant for the mechanical switch through the top hole of the tomato shell. Only after passing the wires through, solder them to your keyboard switch. Once soldered, click the switch into its housing.
5. **Hide the Touch Sensor:** Glue or mount the TTP223 sensor inside the shell, right behind the front face.
6. **Closing Up (Optional):** You can leave the back open for easy access, or seal it up using the backplate, M3 heat-set inserts, and M3 screws.

## Flashing the firmware
The complete source code is written using the Arduino framework via PlatformIO. It handles everything: creating a WiFi setup portal on the first boot, syncing the time via NTP, and hosting a sleek Web UI on your local network to let you set multiple alarms, adjust the volume, and manage days.

You can find the full code, along with flashing instructions here: [GitHub](https://github.com/FallMarsisus/PicoWake)