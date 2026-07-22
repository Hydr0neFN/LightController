# LightController

ESP32-C3 Super Mini based LED controller with USB-C PD input, DC barrel jack/ terminal block input, two PWM-dimmed LED strip channels, and an addressable NeoPixel output. Apple HomeKit native via [HomeSpan](https://github.com/HomeSpan/HomeSpan), with optional MQTT status reporting.

Built as a one-person project for **Year 1 Period 4 — Power up Yourself** at Hanze University of Applied Sciences, Groningen.

## Hardware

Custom PCB designed in KiCad (design files in [`pcb/`](pcb/)).

### Key Components

| Component | Role |
|---|---|
| ESP32-C3 Super Mini | MCU, WiFi, HomeKit bridge |
| CH224K | USB-C PD sink controller (5V/9V/12V/15V/20V negotiation) |
| AP63205WU | Buck converter, PD/DC rail → 5V for MCU |
| SN74LVC1T45 | 3.3V → 5V level shifter for NeoPixel DIN |
| IRFZ44N × 2 | Low-side N-MOSFETs for LED strip channels |
| UCC27423 | Dual gate driver for MOSFET channels |

### Power Input (Up to 24V tolorance)

- **USB-C PD** via CH224K — negotiates 5V/9V/12V/15V/20V from a PD source
- **DC barrel jack** — direct input (e.g. 12/24V wall adapter), bypasses PD negotiation
- **Terminal Block** — direct input if user have PSU input, bypasses PD negotiation.

Both inputs feed the LED strips directly and the AP63205 buck for 5V MCU power.

### Outputs (All shares same power rail)

- **Channel A** — PWM-dimmed LED strip (low-side MOSFET, enable gate)
- **Channel B** — PWM-dimmed LED strip (low-side MOSFET, enable gate)
- **NeoPixel** — addressable RGB string via level-shifted DIN (SN74LVC1T45)

### PCB History

The PCB was originally designed by [Andrei (ihoneypot)](https://github.com/ihoneypot) as a basic LED controller. I modified the design to add:

- USB-C PD input with CH224K voltage negotiation
- DC barrel jack input as alternative power source
- ESP8266 → ESP32-C3 Super Mini swap (HomeKit-capable, smaller footprint)
- Unified power routing: three input paths merged to one rail, split to two LED strip outputs + NeoPixel output
- 3.3V→5V level shifter for NeoPixel data line

### PD Status — Not Working

The CH224K PD negotiation circuit is **currently disabled** (`ENABLE_PD 0` in config). During bringup we burned several CH224K chips and discovered through debugging:

- The 1kΩ resistor from VBUS to VDD acts as a voltage divider but the 0805 package cannot dissipate the power at higher voltages (P = V²/R; at 20V that's 0.4W through a 0.1W-rated resistor — it overheats and eventually burns)
- USB-C D+/D- must be shorted on the chip side for proper enumeration
- The USB-C connector requires careful reflow; loose joints cause intermittent communication

**Fixes needed for a future revision:**
- Use a power resistor rated for ≥0.4W (or ≥0.15W if limiting to 12V max)
- Short D+/D- on the CH224K side, remove the data line routing from the USB-C connector
- The firmware PD controller (`PDController.h`) is implemented but untested on hardware — only the hardware needs fixing before end-to-end testing can happen

## Firmware

PlatformIO project targeting `nologo_esp32c3_super_mini` via the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork (arduino-esp32 core 3.3.9, ESP-IDF 5.5.4).

### Features

- **HomeKit native** — two dimmable lightbulbs (CH A/B) + one RGB lightbulb (NeoPixel), bridged accessory
- **PD voltage switching** — load-shedding before rail transition, NVS persistence for brown-out recovery (disabled until hardware fix)
- **MQTT status** — periodic JSON status publish, network voltage control via `lightctrl/cmd/voltage`
- **Web Log** — live status/log at `http://<device-ip>/status`
- **Serial CLI** — `V0`..`V4` for PD voltage, `J` for status JSON, plus full HomeSpan command set
- **OTA updates** — via HomeSpan's built-in OTA

### Building

1. Copy `src/config.h.example` → `src/config.h`
2. Edit `config.h` — set your WiFi credentials, HomeKit pairing code, MQTT broker IP, and NeoPixel count
3. Build and flash:

```bash
pio run -t upload
pio device monitor
```

### Pin Map

| GPIO | Function |
|---|---|
| 0 | CH B PWM (strapping pin) |
| 1 | CH A PWM |
| 2 | CH A enable |
| 3 | CH B enable |
| 4 | NeoPixel DIN |
| 5 | CH224K CFG1 |
| 6 | CH224K CFG3 |
| 7 | CH224K CFG2 |
| 8 | Status LED (boot strapping) |
| 21 | CH224K PG (Power Good) |

### Project Structure

```
├── src/
│   ├── main.cpp           # Setup, loop, serial commands, status LED
│   ├── config.h.example   # Configuration template (copy to config.h)
│   ├── Lights.h           # DimmableChannel + NeoStrip HomeKit services
│   ├── PDController.h     # CH224K PD voltage negotiation driver
│   └── StatusReporter.h   # MQTT publisher + JSON status builder
├── pcb/                   # KiCad 10 design files
│   ├── LEDController.kicad_sch   # Schematic
│   ├── LEDController.kicad_pcb   # PCB layout
│   ├── LEDController.kicad_pro   # Project file
│   ├── LEDController.csv         # BOM
│   ├── LEDController.pretty/     # Custom footprints (ESP32-C3 module, UCC27423)
│   ├── LedPannel.pretty/        # Custom footprints (PowerDI diode, inductor)
│   └── Prod/                    # Production Gerber files
└── platformio.ini
```

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| [HomeSpan](https://github.com/HomeSpan/HomeSpan) | ≥2.1.0 | HomeKit accessory framework |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | ≥2.8 | MQTT client |

## Credits

- **PCB base design** — [Andrei (ihoneypot)](https://github.com/ihoneypot)
- **Modifications, firmware, bringup** — [Yu-I (Hydr0neFN)](https://github.com/Hydr0neFN)

## License

MIT
