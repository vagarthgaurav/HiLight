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
- Rotary encoder for brightness control (turn) and power toggle (short press)
- Button long-press (2 s) to send a "Hi"
- Hold for 5 s to enter WiFi setup mode

**Light Bulb** — the LED board featuring:
- Filament LEDs for warm white light (PWM dimmed at 20 kHz)
- 20 × WS2813B addressable RGB LEDs for the Hi animations

<img src="images/Hi light.jpeg" width="400" alt="HiLight lamp photo">

KiCad project files are in [Hardware/](Hardware/).

## Software

Firmware is built with PlatformIO (Arduino framework) targeting the ESP32-C3.

### Configuration

WiFi credentials are configured at runtime via the built-in captive portal (see below). No credentials file is needed.

### WiFi setup (captive portal)

Hold the button for **5 seconds** until the lamp pulses orange. Connect to the `HiLight-Setup` WiFi network — a setup page will open automatically where you can select your network and enter the password. The lamp exits setup mode after saving or after 5 minutes.

### Home Assistant

The lamp shows up automatically in Home Assistant as a dimmable light — no manual configuration needed. It supports on/off and brightness control from the HA dashboard, and you can also use it in automations.

Under the hood it connects to an MQTT broker over WebSocket/TLS and publishes discovery config on boot. The broker host is set in `network.cpp`.

OTA updates are triggered by publishing a firmware URL to `hilight/ota`.

### Button behaviour

| Action | Result |
|---|---|
| Short press | Toggle white light on/off |
| Turn encoder | Adjust brightness (logarithmic) |
| Hold 2 s | Send a "Hi" to all paired lamps |
| Hold 5 s | Enter WiFi setup mode |
