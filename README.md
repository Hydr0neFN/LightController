**English** · [繁體中文](README.zh-TW.md)

# LightController

An ESP32-C3 Super Mini LED controller with two PWM-dimmed LED strip channels and one addressable NeoPixel output, controlled from Apple HomeKit. Built as a school project for Year 1 Period 4 — Power up Yourself at Hanze University of Applied Sciences, Groningen.

<img width="460" height="443" alt="image" src="https://github.com/user-attachments/assets/db531565-c83b-4483-a6f2-add53fab505e" />

## At a glance

| | |
|---|---|
| What it is | ESP32-C3 Super Mini LED controller on a custom PCB (KiCad, files in [`pcb/`](pcb/)) |
| Input | USB-C PD, DC barrel jack, or terminal block (up to 24V tolerance) |
| Output | Channel A + Channel B PWM LED strips and a NeoPixel string (all share one power rail) |
| Software | HomeSpan HomeKit native, optional MQTT status, web log, serial CLI, OTA |
| Status | Archived — the PD input stays disabled until a hardware fix (see [Limits and open items](#limits-and-open-items)) |
| Scope | One-person project: PCB modification, firmware, bringup |

## How it works

```
  USB-C PD input ──┐
  DC barrel jack ──┼──> one power rail ──> Channel A
  Terminal block ──┘         │             Channel B
                             │             NeoPixel
                             └──> AP63205 5V buck ──> ESP32-C3 Super Mini
                                                       -> HomeKit via HomeSpan
                                                       -> MQTT status (optional)
```

Three power inputs feed the LED strips directly and the AP63205 buck for 5V MCU power. The PD input negotiates 5V/9V/12V/15V/20V through a CH224K.

- **Channel A / Channel B** — PWM-dimmed LED strip, low-side MOSFET with enable gate, driven by the UCC27423 dual gate driver.
- **NeoPixel** — addressable RGB string on a level-shifted DIN (SN74LVC1T45, 3.3V → 5V).
- **HomeKit** — two dimmable lightbulbs (CH A/B) and one RGB lightbulb (NeoPixel), presented as a bridged accessory.
- **PD voltage switching** — load-shedding before the rail transition, NVS persistence for brown-out recovery. Disabled until the hardware fix.
- **MQTT status** — periodic JSON status publish, network voltage control via `lightctrl/cmd/voltage`.
- **Web log** — live status/log at `http://<device-ip>/status`.
- **Serial CLI** — `V0`..`V4` for PD voltage, `J` for status JSON, plus the full HomeSpan command set.
- **OTA updates** — via HomeSpan's built-in OTA.

## The engineering story

**Symptom.** During bringup we burned several CH224K chips on the PD input.

**Cause.** Debugging turned up three problems:

- The 1kΩ resistor from VBUS to VDD acts as a voltage divider, but the 0805 package cannot dissipate the power at higher voltages. P = V²/R, so at 20V that is 0.4W through a 0.1W-rated resistor: it overheats and eventually burns.
- USB-C D+/D- must be shorted on the chip side for proper enumeration.
- The USB-C connector requires careful reflow; loose joints cause intermittent communication.

**Response.** I disabled the PD circuit (`ENABLE_PD 0` in config). The firmware PD controller (`PDController.h`) is implemented but untested on hardware.

**Fix needed for a future revision.** Use a power resistor rated for ≥0.4W (or ≥0.15W if limiting to 12V max), short D+/D- on the CH224K side, and remove the data line routing from the USB-C connector. Only the hardware needs fixing before end-to-end testing can happen.

## Build / run

PlatformIO project for `nologo_esp32c3_super_mini` with the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork (arduino-esp32 core 3.3.9, ESP-IDF 5.5.4).

1. Copy `src/config.h.example` → `src/config.h`
2. Edit `config.h` — set your WiFi credentials, HomeKit pairing code, MQTT broker IP, and NeoPixel count
3. Build and flash:

```bash
pio run -t upload
pio device monitor
```

## Repo layout

```
├── src/           # firmware: main.cpp, Lights.h, PDController.h, StatusReporter.h
├── pcb/           # KiCad 10 design files and production Gerbers
├── include/, lib/ # PlatformIO stubs
├── test/          # PlatformIO stub
└── platformio.ini
```

[Detailed layout, pin map and dependencies](docs/hardware-and-build.md#repo-layout)

## Limits and open items

The PD input is disabled (`ENABLE_PD 0`). The firmware PD controller is untested on hardware. A resistor rated for ≥0.4W (or ≥0.15W) and the D+/D- short are fixes for a future revision.

[Full hardware notes and PCB history](docs/hardware-and-build.md)

## Credits

- **PCB base design** — [Andrei (ihoneypot)](https://github.com/ihoneypot)
- **Modifications, firmware, bringup** — [Yu-I (Hydr0neFN)](https://github.com/Hydr0neFN)

## License

MIT, as stated in [`LICENSE`](LICENSE).
