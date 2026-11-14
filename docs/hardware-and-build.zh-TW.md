[English](hardware-and-build.md) · **繁體中文**

# 硬體、PCB 沿革與建置細節

回到 [README](../README.zh-TW.md)。

## 主要元件

| 元件 | 角色 |
|---|---|
| ESP32-C3 Super Mini | MCU、WiFi、HomeKit 橋接器 |
| CH224K | USB-C PD sink 控制器（5V/9V/12V/15V/20V 協商） |
| AP63205WU | 降壓轉換器（Buck converter），PD/DC 電軌 → MCU 用的 5V |
| SN74LVC1T45 | 3.3V → 5V 電位轉換器，供 NeoPixel DIN 使用 |
| IRFZ44N × 2 | 低側 N-MOSFET，供 LED 燈條通道使用 |
| UCC27423 | 雙閘極驅動器，供 MOSFET 通道使用 |

## 電源輸入（最高耐受 24V）

- **USB-C PD** 透過 CH224K — 向 PD 供電源協商 5V/9V/12V/15V/20V
- **DC 圓孔插孔** — 直接輸入（例如 12/24V 壁插變壓器），不經 PD 協商
- **端子台** — 使用者已有 PSU 輸入時可直接接，不經 PD 協商

## 輸出

所有輸出共用同一條電源軌。

- **Channel A** — PWM 調光 LED 燈條（低側 MOSFET、致能閘極）
- **Channel B** — PWM 調光 LED 燈條（低側 MOSFET、致能閘極）
- **NeoPixel** — 經電位轉換 DIN 的可定址 RGB 燈條（SN74LVC1T45）

## PCB 沿革

這塊 PCB 原本是 [Andrei (ihoneypot)](https://github.com/ihoneypot) 設計的基本款 LED 控制器。我改了設計，新增：

- 具備 CH224K 電壓協商的 USB-C PD 輸入
- DC 圓孔插孔輸入，可當替代電源
- ESP8266 換成 ESP32-C3 Super Mini（支援 HomeKit、佔板面積更小）
- 統一電源走線：三條輸入路徑併成一條電源軌，再分給兩組 LED 燈條輸出 + NeoPixel 輸出
- 供 NeoPixel 資料線使用的 3.3V→5V 電位轉換器

## PD 狀態 — 尚未正常運作

CH224K 的 PD 協商電路目前停用（設定中的 `ENABLE_PD 0`）。上電除錯時燒掉了好幾顆 CH224K 晶片，除錯後發現：

- 從 VBUS 到 VDD 的 1kΩ 電阻拿來當分壓器用，但 0805 封裝在較高電壓下散不掉功率。P = V²/R，在 20V 時 0.1W 額定的電阻要扛 0.4W，會過熱，最後燒掉。
- USB-C 的 D+/D- 要在晶片端短接，才能正確列舉。
- USB-C 連接器要仔細迴焊；虛焊會讓通訊斷斷續續。

下一版要修的：

- 換成額定 ≥0.4W 的功率電阻（若最高只跑 12V 則 ≥0.15W）
- 在 CH224K 端把 D+/D- 短接，並把 USB-C 連接器過來的資料線走線拿掉
- 韌體的 PD 控制器（`PDController.h`）已經實作，但還沒在硬體上跑過 — 要先修好硬體，才能做端對端測試

## 專案結構

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

## 腳位對應

| GPIO | 功能 |
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

## 相依性

| 函式庫 | 版本 | 用途 |
|---|---|---|
| [HomeSpan](https://github.com/HomeSpan/HomeSpan) | ≥2.1.0 | HomeKit accessory framework |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | ≥2.8 | MQTT client |

## 鳴謝

- **PCB 基礎設計** — [Andrei (ihoneypot)](https://github.com/ihoneypot)
- **修改、韌體、上電除錯** — [Yu-I (Hydr0neFN)](https://github.com/Hydr0neFN)
