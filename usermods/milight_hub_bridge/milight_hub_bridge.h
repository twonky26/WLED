#pragma once

// Shared helpers for interacting with the MiLight RF bridge usermod from core UI code.

#include <vector>

struct MilightBulbConfig {
  String name = F("Primary light");
  String remoteType = F("rgb_cct");
  String colorMode = F("rgb_cct");
  uint16_t deviceId = 0x0001;
  uint8_t groupId = 0x01;
};

struct MilightHubBridgeSettings {
  bool enabled = true;
  bool mirrorStateToRadio = true;
  bool applyRadioToState = true;
  int8_t cePin = -1;
  int8_t csnPin = -1;
  int8_t irqPin = -1;
  int8_t sckPin = -1;
  int8_t misoPin = -1;
  int8_t mosiPin = -1;
  uint8_t rfPowerLevel = 1;       // RF24_PA_LOW
  uint8_t rfChannelPreset = 1;    // mid
  uint8_t listenChannelPreset = 1; // mid
  uint8_t packetRepeats = 3;
  uint8_t packetRepeatsPerLoop = 1;
  uint8_t listenRepeats = 1;
  uint8_t groupId = 0x01;
  uint16_t deviceId = 0x0001;
  unsigned long minSendInterval = 45;
  std::vector<MilightBulbConfig> lights;
  uint8_t activeLight = 0;
};

// Return current MiLight bridge settings. Returns false if the usermod is not active.
bool milightGetSettings(MilightHubBridgeSettings& out);

// Apply settings coming from the LED Output page and optionally trigger a link/unlink action.
// linkAction: 0 = none, 1 = pair/link, 2 = unlink.
bool milightApplySettings(const MilightHubBridgeSettings& cfg, uint8_t linkAction = 0);
