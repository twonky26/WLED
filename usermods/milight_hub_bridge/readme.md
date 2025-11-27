# MiLight RF bridge usermod

Bridge MiLight 2.4 GHz remotes directly to WLED using an nRF24L01 without requiring MQTT.
The usermod mirrors WLED state changes back over RF so MiLight bulbs and remotes stay
synchronized.

## Features
- Uses an nRF24L01 to send and receive MiLight-style packets (on/off, brightness, hue, and white temperature) directly.
- Configurable CE/CSN/SCK/MISO/MOSI/IRQ pins and RF channel for ESP32-C3 builds.
- Places settings in the **Usermod** settings page so wiring, power level, channel presets, and packet repeats can be tuned alongside other usermods.
- Optionally mirror WLED state changes to RF and/or react to packets from MiLight remotes.
- Exposes a virtual **MiLight (RF)** LED output so you can add MiLight bulbs alongside other outputs.

## Configuration
Fields appear under **Usermods** in the WLED UI (JSON-driven form):
- **enabled** – toggle the MiLight RF bridge on/off.
- **mirror_state** – publish WLED state changes to the MiLight radio.
- **listen_radio** – apply MiLight packets to WLED.
- **ce_pin**, **cs_pin**, **irq_pin** – nRF24L01 control pins (IRQ optional).
- **sck_pin**, **miso_pin**, **mosi_pin** – SPI bus pins (set to match your board wiring).
- **rf_power** – RF24 PA level (min/low/high/max).
- **channel** / **listen_channel** – preset RF channels (low/mid/high) for sending vs. listening.
- **packet_repeats**, **packet_repeats_per_loop**, **listen_repeats** – MiLight-style retry counts pulled from esp8266_milight_hub defaults.
- **base_address** – upper four bytes of the 5-byte MiLight address (defaults to `0xB0B1B2B3`; change only if you must match a different hub’s Device ID base).
- **lights** – array of MiLight bulbs/remotes with a **name**, **remote_type**, **device_id**, and **group_id**; select **active_light** to drive from WLED.
- **min_interval_ms** – throttle time between RF transmissions.

### Pairing (matching esp8266_milight_hub)

1. Pick a Device ID (0x0000–0xFFFF) and Group ID (1–4; group 0 addresses all bulbs for that Device ID).
2. Power off the bulb you want to pair.
3. Enter the Device ID and Group ID for a light entry, set it as the **active_light**, and click **pair** (link action) within five seconds of powering the bulb.
4. The light should flash to confirm pairing. Use the **unlink** action to remove a pairing.

Add `milight_hub_bridge` to `custom_usermods` (or copy the folder into `usermods/`) to include
MiLight RF bridging in your WLED build. Provide an nRF24L01 wired to the configured pins.
