# LeLight esphome integration

Adaptation of [Home Assistant LeLight integration](https://github.com/v1ack/lelight)

> ❗️ You need to get lamp ID from app [like this](https://github.com/v1ack/lelight#configuration)

## Installation in esphome

```yaml
# esphome config
esp32:
  board: esp32dev
  framework:
    type: esp-idf # Recommended for BLE instead of arduino

# Load integration
external_components:
  - source: github://v1ack/lelight-esphome
    components: [ ble_lelight ]

# Add a light output
light:
  - platform: ble_lelight # Use the lelight integration
    name: "My light" # Name of the light in Home Assistant
    id: light_lamp # ID of the light in esphome
    lamp_mac: "01234567" # ID of the lamp in the app
    interval: 250ms # How long to send each command, may be increased if something is not working
    tx_power: 9dBm # Optional, default is 9dBm (max), may be decreased to save battery
```

## Recommendations

- Use `esp-idf` because it's more stable with BLE
- Use only one light per ESP32, however, it can be used with multiple lights but not at the same time
- Works only with ESP32 with Bluetooth support
