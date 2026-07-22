#pragma once
#include <Arduino.h>
#include "config.h"
#include "PDController.h"
#include "Lights.h"

#if ENABLE_MQTT
  #include <WiFi.h>
  #include <PubSubClient.h>
#endif

// ============================================================================
//  StatusReporter
//  - Always: HomeSpan Web Log (set up in main) is the primary live status/log
//    surface, viewable at  http://<device-ip>/<WEBLOG_URL> .
//  - Optional: MQTT publish of a JSON status blob on an interval.
// ============================================================================
class StatusReporter {
public:
  // Handler invoked when a voltage command arrives over the network.
  using VoltHandler = void (*)(PDVoltage);

  StatusReporter(PDController *pd, DimmableChannel *a, DimmableChannel *b, NeoStrip *neo)
      : _pd(pd), _a(a), _b(b), _neo(neo) {}

  // Register the function that actually performs the (load-shed) voltage switch.
  void setVoltageHandler(VoltHandler h) { _voltHandler = h; }

  void begin() {
#if ENABLE_MQTT
    _self = this;
    _mqtt.setClient(_wifi);
    _mqtt.setServer(MQTT_HOST, MQTT_PORT);
    _mqtt.setCallback(mqttCallback);
#endif
  }

  // Call from loop().
  void poll() {
#if ENABLE_MQTT
    if (WiFi.status() != WL_CONNECTED) return;
    if (!_mqtt.connected()) reconnect();
    _mqtt.loop();

    uint32_t now = millis();
    if (now - _lastPub >= MQTT_PUBLISH_MS) {
      _lastPub = now;
      publish();
    }
#endif
  }

  // Build the JSON status string into buf. Returns length.
  size_t buildJson(char *buf, size_t n) {
    bool pdOn = PDController::enabled();
    return snprintf(buf, n,
      "{\"pd_mode\":\"%s\",\"pd_v\":\"%s\",\"pd_mv\":%lu,\"pg\":%s,"
      "\"chA\":{\"on\":%s,\"bri\":%d},"
      "\"chB\":{\"on\":%s,\"bri\":%d},"
      "\"neo\":{\"on\":%s,\"bri\":%d,\"hue\":%.0f,\"sat\":%.0f}}",
      pdOn ? "PD" : "DC",
      pdOn ? PDController::voltageName(_pd->current()) : "DC",
      pdOn ? (unsigned long)PDController::voltageMilliVolts(_pd->current()) : 0UL,
      _pd->powerGood() ? "true" : "false",
      _a->isOn() ? "true" : "false", _a->brightness(),
      _b->isOn() ? "true" : "false", _b->brightness(),
      _neo->isOn() ? "true" : "false", _neo->brightness(),
      _neo->hue(), _neo->sat());
  }

private:
#if ENABLE_MQTT
  void reconnect() {
    uint32_t now = millis();
    if (now - _lastTry < 5000) return;   // throttle reconnect attempts
    _lastTry = now;

    const char *user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
    const char *pass = strlen(MQTT_PASS) ? MQTT_PASS : nullptr;
    if (_mqtt.connect(MQTT_BASE_TOPIC "-" HK_HOSTNAME, user, pass,
                      MQTT_BASE_TOPIC "/online", 0, true, "0")) {
      _mqtt.publish(MQTT_BASE_TOPIC "/online", "1", true);
      _mqtt.subscribe(MQTT_BASE_TOPIC "/cmd/voltage");   // net PD-voltage control
      WEBLOG("MQTT connected to %s:%d", MQTT_HOST, MQTT_PORT);
    }
  }

  // PubSubClient callback (static). Payload accepts an enum index 0-4 OR the
  // nominal volts 5/9/12/15/20. Triggers the registered voltage handler, which
  // persists to NVS and sheds load during the switch.
  static void mqttCallback(char *topic, byte *payload, unsigned int len) {
    if (!_self || !_self->_voltHandler) return;
    char b[8] = {0};
    if (len >= sizeof(b)) len = sizeof(b) - 1;
    memcpy(b, payload, len);
    int n = atoi(b);
    PDVoltage v;
    switch (n) {
      case 0:  case 5:  v = PD_5V;  break;   // enum 0 or 5V
      case 1:  case 9:  v = PD_9V;  break;   // enum 1 or 9V
      case 2:  case 12: v = PD_12V; break;   // enum 2 or 12V
      case 3:  case 15: v = PD_15V; break;   // enum 3 or 15V
      case 4:  case 20: v = PD_20V; break;   // enum 4 or 20V
      default: WEBLOG("MQTT voltage cmd invalid: '%s'", b); return;
    }
    WEBLOG("MQTT voltage cmd -> %s", PDController::voltageName(v));
    _self->_voltHandler(v);
  }

  void publish() {
    char buf[256];
    buildJson(buf, sizeof(buf));
    _mqtt.publish(MQTT_BASE_TOPIC "/status", buf, true);
  }

  WiFiClient   _wifi;
  PubSubClient _mqtt;
  uint32_t     _lastPub = 0;
  uint32_t     _lastTry = 0;
  static StatusReporter *_self;   // for the static MQTT callback
#endif

  VoltHandler      _voltHandler = nullptr;
  PDController    *_pd;
  DimmableChannel *_a;
  DimmableChannel *_b;
  NeoStrip        *_neo;
};

#if ENABLE_MQTT
StatusReporter *StatusReporter::_self = nullptr;
#endif
