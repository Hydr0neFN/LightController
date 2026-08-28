[English](README.md) · **繁體中文**

# LightController
<img width="460" height="443" alt="image" src="https://github.com/user-attachments/assets/db531565-c83b-4483-a6f2-add53fab505e" />


以 ESP32-C3 Super Mini 為核心的 LED 控制器，具備 USB-C PD 輸入、DC 圓孔插孔／端子台輸入、兩組 PWM 調光 LED 燈條通道，以及一組可定址 NeoPixel 輸出。透過 [HomeSpan](https://github.com/HomeSpan/HomeSpan) 原生支援 Apple HomeKit，並具備選用的 MQTT 狀態回報功能。

為格羅寧根漢斯應用科技大學（Hanze University of Applied Sciences, Groningen）**Year 1 Period 4 — Power up Yourself** 獨立完成的個人專案。

## 硬體

於 KiCad 中設計的自訂 PCB（設計檔案位於 [`pcb/`](pcb/)）。

### 主要元件

| 元件 | 角色 |
|---|---|
| ESP32-C3 Super Mini | MCU、WiFi、HomeKit 橋接器 |
| CH224K | USB-C PD sink 控制器（5V/9V/12V/15V/20V 協商） |
| AP63205WU | 降壓轉換器（Buck converter），PD/DC 電軌 → MCU 用的 5V |
| SN74LVC1T45 | 3.3V → 5V 電位轉換器，供 NeoPixel DIN 使用 |
| IRFZ44N × 2 | 低側 N-MOSFET，供 LED 燈條通道使用 |
| UCC27423 | 雙閘極驅動器，供 MOSFET 通道使用 |

### 電源輸入（最高耐受 24V）

- **USB-C PD** 透過 CH224K — 向 PD 供電源協商 5V/9V/12V/15V/20V
- **DC 圓孔插孔** — 直接輸入（例如 12/24V 壁插變壓器），繞過 PD 協商
- **端子台** — 若使用者有 PSU 輸入可直接輸入，繞過 PD 協商。

三種輸入直接為 LED 燈條供電，並經由 AP63205 降壓轉換器提供 5V MCU 電源。

### 輸出（全部共用相同電源軌）

- **Channel A** — PWM 調光 LED 燈條（低側 MOSFET、致能閘極）
- **Channel B** — PWM 調光 LED 燈條（低側 MOSFET、致能閘極）
- **NeoPixel** — 透過電位轉換 DIN 的可定址 RGB 燈條（SN74LVC1T45）

### PCB 歷史

此 PCB 最初由 [Andrei (ihoneypot)](https://github.com/ihoneypot) 設計為基本款 LED 控制器。我修改了設計並新增：

- 具備 CH224K 電壓協商的 USB-C PD 輸入
- DC 圓孔插孔輸入作為替代電源
- ESP8266 → ESP32-C3 Super Mini 替換（支援 HomeKit、佔板面積更小）
- 統一電源走線：三條輸入路徑合併為單一電源軌，分流至兩組 LED 燈條輸出 + NeoPixel 輸出
- 供 NeoPixel 資料線使用的 3.3V→5V 電位轉換器

### PD 狀態 — 尚未正常運作

CH224K PD 協商電路**目前已停用**（設定中的 `ENABLE_PD 0`）。在上電除錯過程中我們燒毀了數顆 CH224K 晶片，並透過除錯發現：

- 從 VBUS 到 VDD 的 1kΩ 電阻作為分壓器使用，但 0805 封裝無法在較高電壓下散熱（P = V²/R；在 20V 下通過額定 0.1W 電阻的功率為 0.4W — 其會過熱並最終燒毀）
- USB-C D+/D- 必須在晶片端短接以進行正確列舉
- USB-C 連接器需要仔細進行迴焊；虛焊接點會導致通訊斷斷續續

**未來改版所需修正：**
- 使用額定功率 ≥0.4W 的功率電阻（若限制最高 12V 則為 ≥0.15W）
- 在 CH224K 端短接 D+/D-，並移除來自 USB-C 連接器的資料線走線
- 韌體 PD 控制器（`PDController.h`）已實作但未在硬體上測試 — 需先修復硬體方可進行端對端測試

## 韌體

透過 [pioarduino](https://github.com/pioarduino/platform-espressif32) fork（arduino-esp32 core 3.3.9，ESP-IDF 5.5.4）針對 `nologo_esp32c3_super_mini` 的 PlatformIO 專案。

### 功能特點

- **原生 HomeKit** — 兩組可調光燈泡（CH A/B）+ 一組 RGB 燈泡（NeoPixel），橋接配件
- **PD 電壓切換** — 電源軌切換前先卸載，NVS 持久化儲存以利掉電復原（硬體修復前已停用）
- **MQTT 狀態** — 定期發布 JSON 狀態，透過 `lightctrl/cmd/voltage` 進行網路電壓控制
- **Web 日誌** — 位於 `http://<device-ip>/status` 的即時狀態／日誌
- **序列埠 CLI** — `V0`..`V4` 用於 PD 電壓、`J` 用於狀態 JSON，以及完整的 HomeSpan 指令集
- **OTA 更新** — 透過 HomeSpan 內建的 OTA

### 建置

1. 複製 `src/config.h.example` → `src/config.h`
2. 編輯 `config.h` — 設定您的 WiFi 憑證、HomeKit 配對碼、MQTT broker IP 與 NeoPixel 數量
3. 建置並燒錄：

```bash
pio run -t upload
pio device monitor
```

### 腳位對應

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

### 專案結構

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

## 相依性

| 函式庫 | 版本 | 用途 |
|---|---|---|
| [HomeSpan](https://github.com/HomeSpan/HomeSpan) | ≥2.1.0 | HomeKit accessory framework |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | ≥2.8 | MQTT client |

## 鳴謝

- **PCB 基礎設計** — [Andrei (ihoneypot)](https://github.com/ihoneypot)
- **修改、韌體、上電除錯** — [Yu-I (Hydr0neFN)](https://github.com/Hydr0neFN)

## 授權條款

MIT
