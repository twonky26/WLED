#pragma once
#include "wled.h"

class PIRLDRMod : public Usermod {
  private:
    // PIR sensor parameters
    bool PIRenabled = true;      // Enable/disable PIR sensor
    int PIRpin = 12;             // Default pin for PIR sensor
    int PIRoffSec = 30;          // Time (in seconds) before turning off after no motion
    unsigned long lastPirTrigger = 0; // Timestamp of last PIR trigger
    bool pirState = false;

    // LDR parameters
    bool LDRenable = true;                  // Enable/disable LDR
    int LDRpin = 33;                        // Default pin for LDR
    float LDRReferenceVoltage = 3.3;        // Reference voltage (in volts)
    int LDRAdcPrecision = 12;               // ADC precision (bits)
    int LDRResistorValue = 10000;           // Resistor value (in ohms)
    float LDRLuxOffset = 0.0;               // Offset value for lux measurements
    float luxDeltaThreshold = 10.0;         // Threshold for delta change (in lux)
    uint16_t lastLux = 0;                   // Last lux measurement

    // Helper method to read luminance from the LDR
    uint16_t getLuminance() {
      // Read the analog value from the LDR pin
      float volts = analogRead(LDRpin) * (LDRReferenceVoltage / (1 << LDRAdcPrecision));
      float amps = volts / LDRResistorValue;
      float lux = amps * 1000000 * 2.0; // Conversion factor for lux
      
      // Apply the offset value
      lux += LDRLuxOffset;

      return static_cast<uint16_t>(lux);
    }

    // MQTT discovery
    const char* discoveryTopic = "homeassistant/sensor/wled_%06X/%s/config";

    // Helper method to send sensor data via MQTT
    void sendSensorData(const char* sensorType, int value) {
      if (WLED_MQTT_CONNECTED) {
        char subTopic[64];
        snprintf(subTopic, 64, "wled/%s", sensorType);
        char payload[16];
        snprintf(payload, 16, "%d", value);
        mqtt->publish(subTopic, 0, false, payload);
      }
    }

    // Helper method to discover sensor in HA
    void discoverSensor(const char* sensorType, const char* name, const char* unit) {
      if (WLED_MQTT_CONNECTED) {
        char topic[128];
        char payload[256];
        snprintf(topic, 128, discoveryTopic, escapedMac.c_str(), sensorType);
        snprintf(payload, 256, 
          "{\"name\":\"%s\", \"state_topic\":\"wled/%s\", \"unit_of_measurement\":\"%s\", \"unique_id\":\"wled_%06X_%s\", \"device_class\":\"%s\", \"value_template\":\"{{ value }}\"}", 
          name, sensorType, unit, escapedMac.c_str(), sensorType, sensorType);

        mqtt->publish(topic, 0, false, payload);
      }
    }

  public:
    void setup() {
      // Initialize PIR sensor pin if enabled
      if (PIRenabled) {
        pinMode(PIRpin, INPUT);
      }

      // Initialize LDR pin if enabled
      if (LDRenable) {
        pinMode(LDRpin, INPUT);
      }
      
      // Register the sensors with Home Assistant
      discoverSensor("pir_sensor", "WLED PIR Sensor", "");
      discoverSensor("light_level", "WLED Light Level", "lx");
    }

    void loop() {
      unsigned long now = millis();

      // Handle PIR sensor logic if enabled
      if (PIRenabled) {
        bool pirReading = digitalRead(PIRpin);
        if (pirReading != pirState) {
          pirState = pirReading;
          lastPirTrigger = now;
          // Send the updated PIR state to Home Assistant
          sendSensorData("pir_sensor", pirState);
        }
        // Turn off after the configured period (PIRoffSec) of inactivity
        if (pirState && (now - lastPirTrigger > PIRoffSec * 1000)) {
          pirState = false;
          sendSensorData("pir_sensor", 0);
        }
      }

      // Get the current luminance from the LDR if enabled
      if (LDRenable) {
        uint16_t currentLux = getLuminance();

        // Update and send new lux value if it crosses the delta threshold
        if (abs(currentLux - lastLux) > luxDeltaThreshold) {
          sendSensorData("light_level", currentLux);
          lastLux = currentLux; // Update the last lux value
        }
      }
    }

    // Define the options for the Usermod tab in WLED settings
    void addToConfig(JsonObject &root) {
      JsonObject top = root.createNestedObject("PIRLDRMod");
      top["PIRenabled"] = PIRenabled;
      top["PIRpin"] = PIRpin;
      top["PIRoffSec"] = PIRoffSec;
      top["LDRenable"] = LDRenable; // Add LDR enable to config
      top["LDRpin"] = LDRpin;
      top["LDRReferenceVoltage"] = LDRReferenceVoltage;
      top["LDRAdcPrecision"] = LDRAdcPrecision;
      top["LDRResistorValue"] = LDRResistorValue;
      top["LDRLuxOffset"] = LDRLuxOffset; // Add offset to config
      top["luxDeltaThreshold"] = luxDeltaThreshold; // Add delta threshold to config
    }

    bool readFromConfig(JsonObject &root) {
      JsonObject top = root["PIRLDRMod"];
      if (top.isNull()) return false;

      PIRenabled = top["PIRenabled"] | PIRenabled;
      PIRpin = top["PIRpin"] | PIRpin;
      PIRoffSec = top["PIRoffSec"] | PIRoffSec;
      LDRenable = top["LDRenable"] | LDRenable; // Read LDR enable from config
      LDRpin = top["LDRpin"] | LDRpin;
      LDRReferenceVoltage = top["LDRReferenceVoltage"] | LDRReferenceVoltage;
      LDRAdcPrecision = top["LDRAdcPrecision"] | LDRAdcPrecision;
      LDRResistorValue = top["LDRResistorValue"] | LDRResistorValue;
      LDRLuxOffset = top["LDRLuxOffset"] | LDRLuxOffset; // Read offset from config
      luxDeltaThreshold = top["luxDeltaThreshold"] | luxDeltaThreshold; // Read delta threshold from config

      return true;
    }

    // Provide unique ID for this Usermod
    uint16_t getId() {
      return USERMOD_ID_EXAMPLE;
    }

    // Provide a description for the Usermod settings
    void getUsermodConfig(JsonObject &root) {
      JsonObject top = root.createNestedObject("PIRLDRMod");
      top["PIRenabled"] = PIRenabled;
      top["PIRpin"] = PIRpin;
      top["PIRoffSec"] = PIRoffSec;
      top["LDRenable"] = LDRenable; // Add LDR enable to usermod config
      top["LDRpin"] = LDRpin;
      top["LDRReferenceVoltage"] = LDRReferenceVoltage;
      top["LDRAdcPrecision"] = LDRAdcPrecision;
      top["LDRResistorValue"] = LDRResistorValue;
      top["LDRLuxOffset"] = LDRLuxOffset; // Add offset to usermod config
      top["luxDeltaThreshold"] = luxDeltaThreshold; // Add delta threshold to usermod config
    }

    // Method to get information for the info section
    void addToInfo(String &info) {
      info += "PIR State: " + String(pirState ? "Triggered" : "Not Triggered") + "\n";
      if (LDRenable) {
        uint16_t currentLux = getLuminance();
        info += "LDR Value: " + String(currentLux) + " lx\n";
      }
    }
};
