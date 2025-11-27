#include "wled.h"
#ifdef ARDUINO_ARCH_ESP32
#include <SPI.h>
#include <RF24.h>
#include "usermods/milight_hub_bridge/milight_hub_bridge.h"

// Minimal MiLight-style RF bridge built around an nRF24L01 transceiver.
// This usermod sends WLED state changes as MiLight packets and applies
// MiLight packets received over 2.4 GHz directly to WLED without MQTT.
class MilightHubBridgeUsermod : public Usermod {
  friend bool milightGetSettings(MilightHubBridgeSettings& out);
  friend bool milightApplySettings(const MilightHubBridgeSettings& cfg, uint8_t linkAction);
private:
  bool enabled = true;
  bool mirrorStateToRadio = true;
  bool applyRadioToState = true;
  bool ready = false;

  // Hardware configuration (ESP32-C3 defaults)
  int8_t cePin = 7;   // CE
  int8_t csnPin = 10; // CSN
  int8_t irqPin = -1; // optional
  int8_t sckPin = 4;
  int8_t misoPin = 5;
  int8_t mosiPin = 6;

  uint8_t rfChannel = 83; // MiLight default used by most hubs
  uint32_t baseAddress = 0xB0B1B2B3; // upper 4 bytes of address
  uint8_t groupId = 0x01;            // MiLight group/zone (1-4)
  uint8_t deviceId = 0x01;           // logical bulb id

  uint8_t seq = 1;
  unsigned long lastSend = 0;
  unsigned long minSendInterval = 45; // ms between RF frames

  RF24* radio = nullptr;

  // Field names for config JSON
  static const char _name[];
  static const char _enabled[];
  static const char _mirror[];
  static const char _apply[];
  static const char _cePin[];
  static const char _csnPin[];
  static const char _irqPin[];
  static const char _sckPin[];
  static const char _misoPin[];
  static const char _mosiPin[];
  static const char _channel[];
  static const char _base[];
  static const char _group[];
  static const char _device[];
  static const char _interval[];

  bool ensureRadio() {
    if (radio) return ready;

    SPI.begin(sckPin, misoPin, mosiPin);
    radio = new RF24(cePin, csnPin);
    if (!radio->begin(&SPI, cePin, csnPin)) {
      ready = false;
      return false;
    }

    radio->setChannel(rfChannel);
    radio->setDataRate(RF24_250KBPS);
    radio->setPALevel(RF24_PA_LOW);
    radio->setRetries(1, 5);
    radio->setAutoAck(true);
    radio->enableDynamicPayloads();
    radio->setCRCLength(RF24_CRC_16);

    uint64_t addr = composeAddress();
    radio->openWritingPipe(addr);
    radio->openReadingPipe(1, addr);
    radio->startListening();

    if (irqPin >= 0) {
      pinMode(irqPin, INPUT_PULLUP);
    }

    ready = true;
    return true;
  }

  uint64_t composeAddress() const {
    // MiLight radios use 5-byte addresses. Use baseAddress (4 bytes) + deviceId.
    uint64_t addr = baseAddress;
    addr = (addr << 8) | (deviceId & 0xFF);
    return addr;
  }

  void restartRadioIfNeeded() {
    if (!enabled) return;
    bool shouldRestart = false;
    if (!radio) {
      shouldRestart = true;
    } else if (radio->getChannel() != rfChannel) {
      shouldRestart = true;
    }

    if (shouldRestart) {
      delete radio;
      radio = nullptr;
      ready = false;
      ensureRadio();
    }
  }

  void buildPacket(uint8_t command, uint8_t argument, uint8_t* buffer, uint8_t& len) {
    // Simplified MiLight frame: 7 bytes.
    buffer[0] = 0x7E;          // preamble used by many packet formats
    buffer[1] = deviceId;      // target bulb id
    buffer[2] = groupId;       // group/zone
    buffer[3] = command;       // command identifier
    buffer[4] = argument;      // payload (brightness/color/etc.)
    buffer[5] = seq++;         // sequence
    buffer[6] = 0x00;          // checksum placeholder
    len = 7;
  }

  void sendPacket(uint8_t command, uint8_t argument) {
    if (!enabled || !mirrorStateToRadio || !ensureRadio()) return;
    unsigned long now = millis();
    if (now - lastSend < minSendInterval) return;

    uint8_t payload[7];
    uint8_t len = 0;
    buildPacket(command, argument, payload, len);

    radio->stopListening();
    radio->write(payload, len);
    radio->startListening();
    lastSend = now;
  }

  void sendOnOff(bool on) {
    sendPacket(on ? 0x01 : 0x02, 0x00);
  }

  void sendBrightness(uint8_t briValue) {
    // Map 0-255 to MiLight 1-100 range for arguments.
    uint8_t arg = map(briValue, 0, 255, 0, 100);
    sendPacket(0x03, arg);
  }

  void sendHueFromColor(uint32_t color) {
    // Convert RGB to hue (0-255) and send.
    CHSV hsv = rgb2hsv_approximate(CRGB(R(color), G(color), B(color)));
    sendPacket(0x04, hsv.h);
  }

  void sendWhiteTemperature(uint16_t ct) {
    // ct: 153-500 typical; map to 0-100.
    uint8_t arg = map(ct, 153, 500, 100, 0);
    sendPacket(0x05, arg);
  }

  void handleReceivedPacket(const uint8_t* buf, uint8_t len) {
    if (!applyRadioToState || len < 5) return;
    // Expect preamble 0x7E, device id match (or broadcast 0x00)
    if (buf[0] != 0x7E) return;
    if (buf[1] != deviceId && buf[1] != 0x00) return;
    if (buf[2] != groupId && buf[2] != 0x00) return;

    uint8_t command = buf[3];
    uint8_t argument = buf[4];

    bool changed = false;
    switch (command) {
      case 0x01: // ON
        if (bri == 0) {
          bri = briLast ? briLast : 128;
          changed = true;
        }
        break;
      case 0x02: // OFF
        if (bri > 0) {
          briLast = bri;
          bri = 0;
          changed = true;
        }
        break;
      case 0x03: // Brightness (0-100)
        {
          uint8_t newBri = map(argument, 0, 100, 0, 255);
          if (newBri != bri) {
            bri = newBri;
            changed = true;
          }
        }
        break;
      case 0x04: // Hue
        {
          CHSV hsv;
          hsv.h = argument;
          hsv.s = 255;
          hsv.v = bri > 0 ? bri : 255;
          CRGB newCol;
          hsv2rgb_rainbow(hsv, newCol);

          Segment& seg = strip.getMainSegment();
          uint32_t curColor = seg.colors[0];
          if (R(curColor) != newCol.r || G(curColor) != newCol.g || B(curColor) != newCol.b) {
            colPri[0] = newCol.r;
            colPri[1] = newCol.g;
            colPri[2] = newCol.b;
            seg.setColor(0, RGBW32(newCol.r, newCol.g, newCol.b, W(curColor)));
            changed = true;
          }
        }
        break;
      case 0x05: // White temperature (0-100 mapped to cool/warm)
        {
          // Map to common mired range then convert to segment CCT scale (0-255).
          uint16_t mired = map(argument, 0, 100, 500, 153);
          uint8_t cct = (uint8_t)map(mired, 153, 500, 255, 0);

          Segment& seg = strip.getMainSegment();
          if (seg.cct != cct) {
            seg.setCCT(cct);
            changed = true;
          }
        }
        break;
      default:
        break;
    }

    if (changed) stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  void processRadio() {
    if (!enabled || !applyRadioToState || !ensureRadio()) return;

    while (radio->available()) {
      uint8_t len = radio->getDynamicPayloadSize();
      if (len == 0 || len > 32) {
        radio->flush_rx();
        break;
      }
      uint8_t payload[32];
      radio->read(&payload, len);
      handleReceivedPacket(payload, len);
    }
  }

public:
  MilightHubBridgeUsermod() { instance = this; }
  ~MilightHubBridgeUsermod() {
    if (radio) {
      delete radio;
      radio = nullptr;
    }
  }

  void setup() override {
    if (!enabled) return;
    ensureRadio();
  }

  void loop() override {
    processRadio();
  }

  void onStateChange(uint8_t) override {
    if (!enabled || !mirrorStateToRadio) return;
    Segment& seg = strip.getMainSegment();
    sendOnOff(bri > 0);
    sendBrightness(bri);
    sendHueFromColor(seg.colors[0]);
    sendWhiteTemperature(map(seg.cct, 255, 0, 153, 500));
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray data = user.createNestedArray(FPSTR(_name));
    data.add(enabled ? F("active") : F("disabled"));
    data.add(F("RF channel"));
    data.add(rfChannel);
    if (ready) data.add(F("radio ready"));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject leds = root["leds"];
    if (leds.isNull()) leds = root.createNestedObject("leds");
    JsonObject top = leds.createNestedObject(FPSTR(_name));

    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_mirror)] = mirrorStateToRadio;
    top[FPSTR(_apply)] = applyRadioToState;
    top[FPSTR(_cePin)] = cePin;
    top[FPSTR(_csnPin)] = csnPin;
    top[FPSTR(_irqPin)] = irqPin;
    top[FPSTR(_sckPin)] = sckPin;
    top[FPSTR(_misoPin)] = misoPin;
    top[FPSTR(_mosiPin)] = mosiPin;
    top[FPSTR(_channel)] = rfChannel;
    top[FPSTR(_base)] = baseAddress;
    top[FPSTR(_group)] = groupId;
    top[FPSTR(_device)] = deviceId;
    top[FPSTR(_interval)] = minSendInterval;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject leds = root["leds"];
    if (leds.isNull()) return false;

    JsonObject top = leds[FPSTR(_name)];
    if (top.isNull()) return false;

    bool configComplete = true;
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
    configComplete &= getJsonValue(top[FPSTR(_mirror)], mirrorStateToRadio);
    configComplete &= getJsonValue(top[FPSTR(_apply)], applyRadioToState);
    configComplete &= getJsonValue(top[FPSTR(_cePin)], cePin);
    configComplete &= getJsonValue(top[FPSTR(_csnPin)], csnPin);
    configComplete &= getJsonValue(top[FPSTR(_irqPin)], irqPin);
    configComplete &= getJsonValue(top[FPSTR(_sckPin)], sckPin);
    configComplete &= getJsonValue(top[FPSTR(_misoPin)], misoPin);
    configComplete &= getJsonValue(top[FPSTR(_mosiPin)], mosiPin);
    configComplete &= getJsonValue(top[FPSTR(_channel)], rfChannel);
    configComplete &= getJsonValue(top[FPSTR(_base)], baseAddress);
    configComplete &= getJsonValue(top[FPSTR(_group)], groupId);
    configComplete &= getJsonValue(top[FPSTR(_device)], deviceId);
    configComplete &= getJsonValue(top[FPSTR(_interval)], minSendInterval);

    restartRadioIfNeeded();
    return configComplete;
  }

  uint16_t getId() override { return USERMOD_ID_MILIGHT_HUB_BRIDGE; }
  void applySettings(const MilightHubBridgeSettings& cfg) {
    enabled = cfg.enabled;
    mirrorStateToRadio = cfg.mirrorStateToRadio;
    applyRadioToState = cfg.applyRadioToState;
    cePin = cfg.cePin;
    csnPin = cfg.csnPin;
    irqPin = cfg.irqPin;
    sckPin = cfg.sckPin;
    misoPin = cfg.misoPin;
    mosiPin = cfg.mosiPin;
    rfChannel = cfg.rfChannel;
    baseAddress = cfg.baseAddress;
    groupId = cfg.groupId;
    deviceId = cfg.deviceId;
    minSendInterval = cfg.minSendInterval;
    restartRadioIfNeeded();
  }

  void triggerLink(uint8_t action) {
    if (!action || !ensureRadio()) return;
    // 0x06 and 0x07 are reserved here for pair/unpair style commands.
    if (action == 1) {
      sendPacket(0x06, 0x01);
    } else if (action == 2) {
      sendPacket(0x07, 0x01);
    }
  }
};

const char MilightHubBridgeUsermod::_name[]     PROGMEM = "MiLight";
const char MilightHubBridgeUsermod::_enabled[]  PROGMEM = "enabled";
const char MilightHubBridgeUsermod::_mirror[]   PROGMEM = "mirror_state";
const char MilightHubBridgeUsermod::_apply[]    PROGMEM = "listen_radio";
const char MilightHubBridgeUsermod::_cePin[]    PROGMEM = "ce_pin";
const char MilightHubBridgeUsermod::_csnPin[]   PROGMEM = "cs_pin";
const char MilightHubBridgeUsermod::_irqPin[]   PROGMEM = "irq_pin";
const char MilightHubBridgeUsermod::_sckPin[]   PROGMEM = "sck_pin";
const char MilightHubBridgeUsermod::_misoPin[]  PROGMEM = "miso_pin";
const char MilightHubBridgeUsermod::_mosiPin[]  PROGMEM = "mosi_pin";
const char MilightHubBridgeUsermod::_channel[]  PROGMEM = "channel";
const char MilightHubBridgeUsermod::_base[]     PROGMEM = "base_address";
const char MilightHubBridgeUsermod::_group[]    PROGMEM = "group_id";
const char MilightHubBridgeUsermod::_device[]   PROGMEM = "device_id";
const char MilightHubBridgeUsermod::_interval[] PROGMEM = "min_interval_ms";

static MilightHubBridgeUsermod milight_hub_bridge;
REGISTER_USERMOD(milight_hub_bridge);

static MilightHubBridgeUsermod* instance = nullptr;

bool milightGetSettings(MilightHubBridgeSettings& out) {
  if (!instance) return false;
  out.enabled = instance->enabled;
  out.mirrorStateToRadio = instance->mirrorStateToRadio;
  out.applyRadioToState = instance->applyRadioToState;
  out.cePin = instance->cePin;
  out.csnPin = instance->csnPin;
  out.irqPin = instance->irqPin;
  out.sckPin = instance->sckPin;
  out.misoPin = instance->misoPin;
  out.mosiPin = instance->mosiPin;
  out.rfChannel = instance->rfChannel;
  out.baseAddress = instance->baseAddress;
  out.groupId = instance->groupId;
  out.deviceId = instance->deviceId;
  out.minSendInterval = instance->minSendInterval;
  return true;
}

bool milightApplySettings(const MilightHubBridgeSettings& cfg, uint8_t linkAction) {
  if (!instance) return false;
  instance->applySettings(cfg);
  instance->triggerLink(linkAction);
  return true;
}

#else
#warning "MilightHubBridgeUsermod requires ESP32 platform."
#endif
