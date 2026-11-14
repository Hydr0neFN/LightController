[English](README.md) · **繁體中文**

# LightController

以 ESP32-C3 Super Mini 為核心的 LED 控制器，有兩組 PWM 調光 LED 燈條通道和一組可定址 NeoPixel 輸出，可以從 Apple HomeKit 控制。這是格羅寧根漢斯應用科技大學（Hanze University of Applied Sciences, Groningen）**Year 1 Period 4 — Power up Yourself** 的校內專案。

<img width="460" height="443" alt="image" src="https://github.com/user-attachments/assets/db531565-c83b-4483-a6f2-add53fab505e" />

## 一覽

| | |
|---|---|
| 是什麼 | 自訂 PCB 上的 ESP32-C3 Super Mini LED 控制器（KiCad 設計檔在 [`pcb/`](pcb/)） |
| 輸入 | USB-C PD、DC 圓孔插孔、端子台（最高耐受 24V） |
| 輸出 | Channel A + Channel B 兩組 PWM LED 燈條、NeoPixel 燈條（全部共用同一電源軌） |
| 軟體 | HomeSpan 原生 HomeKit、選用 MQTT 狀態回報、Web 日誌、序列埠 CLI、OTA |
| 狀態 | 已封存 — PD 輸入在硬體修好前維持停用（見[限制與待辦](#限制與待辦)） |
| 範圍 | 一人專案：PCB 修改、韌體、上電除錯 |

## 運作方式

```
  USB-C PD input ──┐
  DC barrel jack ──┼──> one power rail ──> Channel A
  Terminal block ──┘         │             Channel B
                             │             NeoPixel
                             └──> AP63205 5V buck ──> ESP32-C3 Super Mini
                                                       -> HomeKit via HomeSpan
                                                       -> MQTT status (optional)
```

三路輸入直接替 LED 燈條供電，也經由 AP63205 降壓轉換器供 5V 給 MCU。PD 輸入靠 CH224K 協商 5V/9V/12V/15V/20V。

- **Channel A / Channel B** — PWM 調光 LED 燈條，低側 MOSFET 加致能閘極，由 UCC27423 雙閘極驅動器推動。
- **NeoPixel** — 可定址 RGB 燈條，DIN 經 3.3V → 5V 電位轉換（SN74LVC1T45）。
- **HomeKit** — 兩組可調光燈泡（CH A/B）加一組 RGB 燈泡（NeoPixel），以橋接配件呈現。
- **PD 電壓切換** — 切電源軌前先卸載，並用 NVS 持久化，斷電後好復原。硬體修好前停用。
- **MQTT 狀態** — 定期發布 JSON 狀態，可透過 `lightctrl/cmd/voltage` 遠端切電壓。
- **Web 日誌** — 即時狀態／日誌在 `http://<device-ip>/status`。
- **序列埠 CLI** — `V0`..`V4` 切 PD 電壓、`J` 印狀態 JSON，另外還有完整的 HomeSpan 指令集。
- **OTA 更新** — 用 HomeSpan 內建的 OTA。

## 除錯經過

**現象。** 上電除錯時燒掉好幾顆 CH224K 晶片，全都壞在 PD 輸入。

**原因。** 追下來有三個問題：

- 從 VBUS 到 VDD 的 1kΩ 電阻拿來當分壓器用，但 0805 封裝在較高電壓下散不掉功率。P = V²/R，在 20V 時 0.1W 額定的電阻要扛 0.4W，會過熱，最後燒掉。
- USB-C 的 D+/D- 要在晶片端短接，才能正確列舉。
- USB-C 連接器要仔細迴焊；虛焊會讓通訊斷斷續續。

**當下處理。** 先把 PD 電路停用（設定中的 `ENABLE_PD 0`）。韌體的 PD 控制器（`PDController.h`）已經實作，但還沒在硬體上跑過。

**下一版要修的。** 換成額定 ≥0.4W 的功率電阻（若最高只跑 12V 則 ≥0.15W），在 CH224K 端把 D+/D- 短接，並把 USB-C 連接器過來的資料線走線拿掉。硬體修好之前，端對端測試做不了。

## 建置與執行

PlatformIO 專案，板子是 `nologo_esp32c3_super_mini`，用 [pioarduino](https://github.com/pioarduino/platform-espressif32) fork（arduino-esp32 core 3.3.9、ESP-IDF 5.5.4）。

1. 複製 `src/config.h.example` → `src/config.h`
2. 編輯 `config.h` — 填 WiFi 憑證、HomeKit 配對碼、MQTT broker IP 與 NeoPixel 數量
3. 建置並燒錄：

```bash
pio run -t upload
pio device monitor
```

## 專案結構

```
├── src/           # 韌體：main.cpp, Lights.h, PDController.h, StatusReporter.h
├── pcb/           # KiCad 10 設計檔與量產 Gerber
├── include/, lib/ # PlatformIO 佔位目錄
├── test/          # PlatformIO 佔位目錄
└── platformio.ini
```

[完整目錄結構、腳位對應與相依性](docs/hardware-and-build.zh-TW.md#專案結構)

## 限制與待辦

PD 輸入停用中（`ENABLE_PD 0`）。韌體的 PD 控制器還沒在硬體上測過。額定 ≥0.4W（或 ≥0.15W）的電阻和 D+/D- 短接，留待下一版修正。

[硬體細節與 PCB 沿革](docs/hardware-and-build.zh-TW.md)

## 致謝

- **PCB 基礎設計** — [Andrei (ihoneypot)](https://github.com/ihoneypot)
- **修改、韌體、上電除錯** — [Yu-I (Hydr0neFN)](https://github.com/Hydr0neFN)

## 授權條款

MIT，內容見 [`LICENSE`](LICENSE)。
