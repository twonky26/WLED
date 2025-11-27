# MiLight RF bridge usermod

Bridge MiLight 2.4 GHz remotes directly to WLED using an nRF24L01 without requiring MQTT.
The usermod mirrors WLED state changes back over RF so MiLight bulbs and remotes stay
synchronized.

## Features
- Uses an nRF24L01 to send and receive MiLight-style packets (on/off, brightness, hue, and white temperature) directly.
- Configurable CE/CSN/SCK/MISO/MOSI/IRQ pins and RF channel for ESP32-C3 builds.
- Places settings in the **LED Output → MiLight** section for convenient wiring and radio tweaks.
- Optionally mirror WLED state changes to RF and/or react to packets from MiLight remotes.

## Configuration
Fields appear under **LED Output → MiLight** in the WLED UI:
- **enabled** – toggle the MiLight RF bridge on/off.
- **mirror_state** – publish WLED state changes to the MiLight radio.
- **listen_radio** – apply MiLight packets to WLED.
- **ce_pin**, **cs_pin**, **irq_pin** – nRF24L01 control pins (IRQ optional).
- **sck_pin**, **miso_pin**, **mosi_pin** – SPI bus pins (set to match your board wiring).
- **channel** – RF channel number (default 83).
- **base_address** – upper four bytes of the 5-byte MiLight address.
- **group_id** – MiLight group/zone to target.
- **device_id** – MiLight device/bulb id used in the address.
- **min_interval_ms** – throttle time between RF transmissions.

Add `milight_hub_bridge` to `custom_usermods` (or copy the folder into `usermods/`) to include
MiLight RF bridging in your WLED build. Provide an nRF24L01 wired to the configured pins.
