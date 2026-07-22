#pragma once
#include "HomeSpan.h"       // also pulls in src/extras/Pixel.h (NeoPixel driver)
#include "config.h"

// Forward decl so a status reporter can read live state.
class DimmableChannel;
class NeoStrip;

// ============================================================================
//  DimmableChannel - one PWM-dimmed LED strip channel (CH A or CH B)
//
//  HomeKit LightBulb with On + Brightness. PWM duty = brightness (low-side
//  MOSFET gate). The EN pin gates the channel on/off independently of duty so
//  the strip is fully dark when off (duty 0 AND enable low).
// ============================================================================
class DimmableChannel : public Service::LightBulb {
public:
  DimmableChannel(uint8_t pwmPin, uint8_t enPin, const char *name)
      : Service::LightBulb(), _enPin(enPin), _name(name) {

    new Characteristic::Name(name);

    _power = new Characteristic::On(false, true);          // persisted to NVS
    _level = new Characteristic::Brightness(100, true);    // persisted to NVS
    _level->setRange(1, 100, 1);

    pinMode(_enPin, OUTPUT);
    digitalWrite(_enPin, LOW);
    _led = new LedPin(pwmPin, 0, PWM_FREQ_HZ);

    apply();   // drive hardware to restored state
  }

  boolean update() override {
    apply();
    return true;
  }

  bool isOn()        const { return _power->getVal(); }
  int  brightness()  const { return _level->getVal(); }
  const char *name() const { return _name; }

  // Physically drop the channel (EN low, duty 0) WITHOUT changing the HomeKit
  // characteristics. Used to shed load during a PD voltage transition.
  void hardOff() { digitalWrite(_enPin, LOW); _led->set(0); }
  // Re-drive the hardware from the current characteristic state.
  void refresh() { apply(); }

private:
  void apply() {
    bool on = _power->getNewVal();
    int  b  = _level->getNewVal();
    digitalWrite(_enPin, on ? HIGH : LOW);
    _led->set(on ? b : 0);
    WEBLOG("%s -> %s %d%%", _name, on ? "ON" : "OFF", b);
  }

  Characteristic::On         *_power;
  Characteristic::Brightness *_level;
  LedPin     *_led;
  uint8_t     _enPin;
  const char *_name;
};

// ============================================================================
//  NeoStrip - addressable RGB string (whole-string single colour)
//
//  HomeKit LightBulb with On + Hue + Saturation + Brightness. Drives every
//  pixel to the same HSV colour. DIN is level-shifted by the SN74LVC1T45.
// ============================================================================
class NeoStrip : public Service::LightBulb {
public:
  NeoStrip(uint8_t din, int nPixels, const char *name = "Neopixel")
      : Service::LightBulb(), _n(nPixels) {

    new Characteristic::Name(name);

    _power = new Characteristic::On(false, true);
    _hue   = new Characteristic::Hue(0, true);
    _sat   = new Characteristic::Saturation(0, true);
    _val   = new Characteristic::Brightness(100, true);
    _val->setRange(1, 100, 1);

    _px = new Pixel(din);
    apply();
  }

  boolean update() override {
    apply();
    return true;
  }

  bool isOn()       const { return _power->getVal(); }
  int  brightness() const { return _val->getVal(); }
  float hue()       const { return _hue->getVal<float>(); }
  float sat()       const { return _sat->getVal<float>(); }

private:
  void apply() {
    bool  on = _power->getNewVal();
    float h  = _hue->getNewVal<float>();
    float s  = _sat->getNewVal<float>();
    float v  = on ? _val->getNewVal<float>() : 0.0f;
    _px->set(Pixel::HSV(h, s, v), _n);
    WEBLOG("Neopixel -> %s H%.0f S%.0f V%.0f", on ? "ON" : "OFF", h, s, v);
  }

  Characteristic::On         *_power;
  Characteristic::Hue        *_hue;
  Characteristic::Saturation *_sat;
  Characteristic::Brightness *_val;
  Pixel *_px;
  int    _n;
};
