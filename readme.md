# HiLight

A bedside lamp that doubles as a way to connect with loved ones through colourful light. Press and hold the button to send a "Hi", the recipient's lamp glows in your colour.

<img src="images/white light.gif" width="400" alt="HiLight lamp showing white light">

## How it works

Each HiLight lamp is assigned a unique colour. When you send a "Hi", all paired lamps glow with your colour. When someone sends one back, your lamp glows in their colour.

<img src="images/sending hi.gif" width="400" alt="Sending a Hi">

## Hardware

The project consists of two custom PCBs:

**Light Board** — the main controller board featuring:
- ESP32-C3 microcontroller
- Rotary encoder for brightness control (turn) and light toggle (short press)
- Button long-press (2 s) to send a "Hi"
- Hold for 5 s to enter WiFi setup mode

**Light Bulb** — the LED board featuring:
- Filament LEDs for warm white light (PWM dimmed at 20 kHz)
- 20 × WS2813B addressable RGB LEDs for the Hi indication

<img src="images/Hi light.jpeg" width="400" alt="HiLight lamp photo">

## Software

Firmware is built with PlatformIO (Arduino framework) targeting the ESP32-C3.

### WiFi setup (captive portal)

To setup WiFi hold the button for **5 seconds** until the lamp pulses orange. Connect to the `HiLight-Setup` WiFi network — a setup page will open automatically where you can select your network and enter the password. The lamp exits setup mode after saving or after 5 minutes.

### Home Assistant

The lamp shows up automatically in Home Assistant as a dimmable light — no manual configuration needed. It supports on/off and brightness control from the HA dashboard, and you can also use it in automations.

### Button behaviour

| Action | Result |
|---|---|
| Short press | Toggle white light on/off |
| Turn encoder | Adjust brightness (logarithmic) |
| Hold 2 s | Send a "Hi" to all paired lamps |
| Hold 5 s | Enter WiFi setup mode |
