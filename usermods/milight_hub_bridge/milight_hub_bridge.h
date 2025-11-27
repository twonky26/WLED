#pragma once

// Shared helpers for interacting with the MiLight RF bridge usermod from core UI code.

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
  uint8_t rfChannel = 83;
  uint32_t baseAddress = 0xB0B1B2B3;
  uint8_t groupId = 0x01;
  uint8_t deviceId = 0x01;
  unsigned long minSendInterval = 45;
};

// Return current MiLight bridge settings. Returns false if the usermod is not active.
bool milightGetSettings(MilightHubBridgeSettings& out);

// Apply settings coming from the LED Output page and optionally trigger a link/unlink action.
// linkAction: 0 = none, 1 = pair/link, 2 = unlink.
bool milightApplySettings(const MilightHubBridgeSettings& cfg, uint8_t linkAction = 0);
