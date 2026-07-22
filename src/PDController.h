#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
//  CH224K USB-PD sink controller
//
//  CFG truth table (requested voltage), CFG bit meaning: 1 = high, 0 = low.
//    5V : CFG1=1  CFG2=x  CFG3=x
//    9V : CFG1=0  CFG2=0  CFG3=0
//    12V: CFG1=0  CFG2=0  CFG3=1
//    15V: CFG1=0  CFG2=1  CFG3=1
//    20V: CFG1=0  CFG2=1  CFG3=0
//
//  Wiring: each CFG line is GPIO -> 1k -> CH224K, no external pull resistors.
//  Drive convention: push-pull through the 1k series, "1" = HIGH, "0" = LOW.
//  Active drive is used (rather than high-Z) because the lines have no external
//  pull; the 1k limits current into the CH224K CFG inputs.
//
//  PG (Power Good): open-drain, active LOW. LOW => requested rail is present.
// ============================================================================

enum PDVoltage : uint8_t { PD_5V = 0, PD_9V = 1, PD_12V = 2, PD_15V = 3, PD_20V = 4 };

class PDController {
public:
  static constexpr bool enabled() { return ENABLE_PD; }

  void begin() {
    if (!enabled()) {
      // DC-only: leave CFG pins high-impedance, don't touch the CH224K, and
      // skip PG (no PD source => PG would float and read as a fault).
      pinMode(PIN_CFG1, INPUT);
      pinMode(PIN_CFG2, INPUT);
      pinMode(PIN_CFG3, INPUT);
      return;
    }

    pinMode(PIN_PG, INPUT_PULLUP);  // external 10k pull-up already present

    _prefs.begin("pd", false);
#if PD_FORCE_DEFAULT
    // Hard override: always boot at the compile-time constant.
    _current = (PDVoltage)PD_DEFAULT_VOLTAGE;
    _prefs.putUChar("volt", (uint8_t)_current);
#else
    // Restore last requested voltage (survives a brown-out reset mid-switch).
    _current = (PDVoltage)_prefs.getUChar("volt", (uint8_t)PD_DEFAULT_VOLTAGE);
#endif
    applyCfg(_current);
  }

  PDVoltage current() const { return _current; }

  // PG is active LOW. In DC-only mode there is no PD source, so report "good"
  // unconditionally (prevents a false fault on the status LED / status JSON).
  bool powerGood() const {
    if (!enabled()) return true;
    return digitalRead(PIN_PG) == LOW;
  }

  // Request a new voltage. Persists FIRST so a transient reset recovers to the
  // intended rail. Returns true if PG asserted within PD_PG_TIMEOUT_MS.
  // NOTE: caller should drop the LED loads (EN low) before calling, to ease the
  // transient on the shared 5V buck. See main.cpp setVoltageCmd().
  bool setVoltage(PDVoltage v) {
    if (!enabled()) return true;           // DC-only: no-op
    _prefs.putUChar("volt", (uint8_t)v);   // persist before touching the rail
    applyCfg(v);
    _current = v;
    return waitPowerGood(PD_PG_TIMEOUT_MS);
  }

  bool waitPowerGood(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    while (!powerGood()) {
      if (millis() - t0 > timeoutMs) return false;
      delay(2);
    }
    return true;
  }

  static const char *voltageName(PDVoltage v) {
    switch (v) {
      case PD_5V:  return "5V";
      case PD_9V:  return "9V";
      case PD_12V: return "12V";
      case PD_15V: return "15V";
      case PD_20V: return "20V";
    }
    return "?";
  }

  // Approximate nominal millivolts, for status reporting.
  static uint32_t voltageMilliVolts(PDVoltage v) {
    switch (v) {
      case PD_5V:  return 5000;
      case PD_9V:  return 9000;
      case PD_12V: return 12000;
      case PD_15V: return 15000;
      case PD_20V: return 20000;
    }
    return 0;
  }

private:
  // Set CFG pins for the requested voltage (1 = float high, 0 = drive low).
  void applyCfg(PDVoltage v) {
    uint8_t c1, c2, c3;
    switch (v) {
      case PD_5V:  c1 = 1; c2 = 1; c3 = 1; break;  // CFG2/3 are don't-care -> float
      case PD_9V:  c1 = 0; c2 = 0; c3 = 0; break;
      case PD_12V: c1 = 0; c2 = 0; c3 = 1; break;
      case PD_15V: c1 = 0; c2 = 1; c3 = 1; break;
      case PD_20V: c1 = 0; c2 = 1; c3 = 0; break;
      default:     c1 = 1; c2 = 1; c3 = 1; break;  // safest fallback -> 5V
    }
    cfgWrite(PIN_CFG1, c1);
    cfgWrite(PIN_CFG2, c2);
    cfgWrite(PIN_CFG3, c3);
  }

  // CFG1..3 are GPIO -> 1k -> CH224K with no external pull resistors.
  // Drive push-pull through the 1k series: bit==1 -> HIGH, bit==0 -> LOW.
  // Active drive (not high-Z) gives a deterministic level on lines that have no
  // external pull; the 1k limits current into the CH224K CFG inputs.
  static void cfgWrite(uint8_t pin, uint8_t bit) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, bit ? HIGH : LOW);
  }

  Preferences _prefs;
  PDVoltage   _current = PD_DEFAULT_VOLTAGE;
};
