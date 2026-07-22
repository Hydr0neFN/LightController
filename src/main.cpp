// ============================================================================
//  LightController - ESP32-C3 Super Mini
//
//  - PD/DC dual input; CH224K negotiates the PD rail from a user-selected
//    voltage (persisted in NVS so a brown-out mid-switch self-recovers).
//  - Two PWM-dimmed LED strip channels (CH A / CH B) as HomeKit lightbulbs.
//  - One NeoPixel string (RGB) as a HomeKit lightbulb.
//  - HomeSpan Web Log for live status/log over HTTP (replaces telnet):
//        http://<device-ip>/status
//  - Optional MQTT status publishing (see config.h ENABLE_MQTT).
//
//  Serial CLI (115200): type 'V0'..'V4' to switch PD voltage
//    0=5V 1=9V 2=12V 3=15V 4=20V   ('?' for full HomeSpan command list)
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include "HomeSpan.h"

#if PD_DISABLE_BROWNOUT
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

#include "config.h"
#include "PDController.h"
#include "Lights.h"
#include "StatusReporter.h"

// ---- Globals ---------------------------------------------------------------
PDController     pd;
DimmableChannel *chA = nullptr;
DimmableChannel *chB = nullptr;
NeoStrip        *neo = nullptr;
StatusReporter  *status = nullptr;

// ---- Switch PD voltage with load-shedding (shared by serial + MQTT) ----------
void switchVoltage(PDVoltage v) {
  if (!PDController::enabled()) {
    WEBLOG("Voltage cmd ignored: PD nego disabled (DC-only)");
    Serial.printf("PD disabled (DC-only) - voltage command ignored\n");
    return;
  }
  WEBLOG("PD switch -> %s (shedding LED load)", PDController::voltageName(v));
  chA->hardOff();
  chB->hardOff();
  delay(20);                        // let load drop before the rail moves

  bool ok = pd.setVoltage(v);       // persists to NVS first, drives CFG, waits PG
  delay(50);                        // let the new rail settle

  chA->refresh();
  chB->refresh();

  if (neo->isOn() && v != PD_5V)
    WEBLOG("WARNING: NeoPixel rail is now %s, not 5V", PDController::voltageName(v));

  WEBLOG("PD now %s, PG=%s", PDController::voltageName(pd.current()),
         ok ? "good" : "TIMEOUT");
  Serial.printf("PD -> %s  PG=%s\n", PDController::voltageName(pd.current()),
                ok ? "good" : "TIMEOUT");
}

// ---- Serial command: switch PD voltage --------------------------------------
void setVoltageCmd(const char *buf) {
  int n = atoi(buf + 1);            // buf is e.g. "V4"
  if (n < PD_5V || n > PD_20V) {
    Serial.printf("Usage: V<0-4>  (0=5V 1=9V 2=12V 3=15V 4=20V)\n");
    return;
  }
  switchVoltage((PDVoltage)n);
}

// ---- Serial command: print status JSON --------------------------------------
void statusCmd(const char *buf) {
  char j[256];
  status->buildJson(j, sizeof(j));
  Serial.println(j);
}

// ---- On-board status LED -----------------------------------------------------
// OFF when normal. Blinks fast on PD power-good fault, slow on WiFi loss.
static inline void ledDrive(bool lit) {
  digitalWrite(PIN_STATUS_LED, lit ? STATUS_LED_ON_LEVEL : !STATUS_LED_ON_LEVEL);
}

void updateStatusLed() {
  static uint32_t last = 0;
  static bool     on   = false;

  uint32_t period;
  if (!pd.powerGood())                    period = STATUS_BLINK_PG_MS;    // priority
  else if (WiFi.status() != WL_CONNECTED) period = STATUS_BLINK_WIFI_MS;
  else { on = false; ledDrive(false); return; }                          // normal: off

  uint32_t now = millis();
  if (now - last >= period) {
    last = now;
    on = !on;
    ledDrive(on);
  }
}

void setup() {
#if PD_DISABLE_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);   // disable BOD before anything else
#endif

  pinMode(PIN_STATUS_LED, OUTPUT);
  ledDrive(false);                              // start dark

  Serial.begin(115200);

  // Negotiate the PD rail first so downstream loads see the intended voltage.
  pd.begin();

  // ---- HomeSpan / WiFi ----
  homeSpan.setWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
  homeSpan.setPairingCode(HK_PAIRING_CODE);
  homeSpan.enableOTA();
  homeSpan.enableWebLog(WEBLOG_MAX_ENTRIES, NTP_SERVER, TIME_ZONE, WEBLOG_URL);

  homeSpan.begin(Category::Bridges, HK_DEVICE_NAME, HK_HOSTNAME);

  // Bridge accessory
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(HK_DEVICE_NAME);

  // CH A
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(HK_NAME_CHA);
    chA = new DimmableChannel(PIN_CHA_PWM, PIN_CHA_EN, HK_NAME_CHA);

  // CH B
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(HK_NAME_CHB);
    chB = new DimmableChannel(PIN_CHB_PWM, PIN_CHB_EN, HK_NAME_CHB);

  // NeoPixel
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(HK_NAME_NEO);
    neo = new NeoStrip(PIN_NEOPIXEL_DIN, NEOPIXEL_COUNT, HK_NAME_NEO);

  // Status reporter (MQTT + JSON helper). Wire network voltage control.
  status = new StatusReporter(&pd, chA, chB, neo);
  status->setVoltageHandler(switchVoltage);
  status->begin();

  // Custom serial commands
  new SpanUserCommand('V', "<0-4> - set PD voltage (0=5V 1=9V 2=12V 3=15V 4=20V)",
                      setVoltageCmd);
  new SpanUserCommand('J', "- print status JSON", statusCmd);

  if (PDController::enabled()) {
    WEBLOG("Boot complete. PD=%s PG=%s", PDController::voltageName(pd.current()),
           pd.powerGood() ? "good" : "BAD");
  } else {
    WEBLOG("Boot complete. DC-only mode (PD nego disabled)");
  }
}

void loop() {
  homeSpan.poll();
  status->poll();
  updateStatusLed();
}
