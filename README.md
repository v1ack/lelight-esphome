# LeLight ESPHome integration

ESPHome integration for controlling LeLight-compatible BLE lamps.

Based on the [Home Assistant LeLight integration](https://github.com/v1ack/lelight).

The ESP32 controls the lamp by broadcasting BLE advertising packets that reproduce the protocol used by the original mobile application. A direct BLE connection to the lamp is not required.

> ❗ You need to extract the lamp ID from the original application. See the [LeLight configuration instructions](https://github.com/v1ack/lelight#configuration).

## Installation

Add the component through `external_components`:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

esphome:
  name: lelight-controller

external_components:
  - source: github://v1ack/lelight-esphome
    components:
      - ble_lelight

light:
  - platform: ble_lelight
    name: "My light"
    id: light_lamp

    # Four-byte lamp ID extracted from the original app.
    # Must contain exactly eight hexadecimal characters.
    lamp_mac: "01234567"

    # How long each command packet is advertised.
    # Longer values can improve reliability in noisy 2.4 GHz environments.
    interval: 250ms

    # Number of times each command packet is advertised.
    # The same packet is reused for every repeat.
    repeats: 2

    # BLE advertising transmission power.
    # Optional, defaults to 9 dBm.
    tx_power: 9dBm
```

## Configuration options

### `lamp_mac`

Required.

The four-byte lamp identifier extracted from the original application.

Despite the historical option name, this is not a six-byte Bluetooth MAC address.

The value must contain exactly eight hexadecimal characters:

```yaml
lamp_mac: "01234567"
```

### `interval`

Optional. Default: `250ms`.

Controls how long each command packet is advertised before the component proceeds to the next command or repeat.

Allowed range:

```text
200ms–500ms
```

Increase this value if the lamp occasionally misses commands:

```yaml
interval: 400ms
```

A larger value improves the chance that the lamp receives the packet, but makes state changes slower.

### `repeats`

Optional. Default: `2`.

Controls how many times each command packet is advertised.

```yaml
repeats: 3
```

The integration generates the packet once and repeats the same packet, including its message ID and random prefix. This represents retransmission of one command rather than several separate logical commands.

Increasing `repeats` may improve reliability, but also increases the time required to apply a complete state change.

When turning the lamp on, ESPHome normally sends three commands:

1. Power on
2. Color temperature
3. Brightness

Therefore, higher `interval` and `repeats` values can make the complete update noticeably slower.

### `tx_power`

Optional. Default: `9dBm`.

BLE advertising transmission power:

```yaml
tx_power: 9dBm
```

Lower values may reduce power consumption and radio interference. For a mains-powered ESP32 controller, leaving the default maximum power is usually appropriate.

## Reliability tuning

Start with:

```yaml
interval: 250ms
repeats: 2
tx_power: 9dBm
```

If the lamp occasionally ignores commands, try increasing `repeats` first:

```yaml
repeats: 3
```

If commands are still missed, increase `interval`:

```yaml
interval: 400ms
repeats: 3
```

Avoid increasing both values unnecessarily, because every power, brightness, and color-temperature command is sent separately.

## Framework

ESP-IDF is recommended:

```yaml
esp32:
  framework:
    type: esp-idf
```

The component uses the ESP-IDF BLE GAP advertising API directly.

Recent ESPHome versions also use ESP-IDF internally when the Arduino framework is selected, but ESP-IDF remains the best-tested configuration for this integration.

## Limitations

* Only ESP32 variants with Bluetooth LE support are compatible.
* The integration sends commands through BLE advertising and receives no acknowledgement from the lamp.
* The controller cannot know with certainty whether a command was received.
* Multiple lamps may be configured, but simultaneous command transmission is not supported reliably because all instances share the same ESP32 BLE advertiser.
* Other ESPHome components that actively use BLE advertising may conflict with this integration.
* Rapid intermediate brightness or color-temperature changes may be collapsed so that the most recent requested state is sent.
* The integration currently supports:

  * power on and off;
  * brightness;
  * color temperature.

## One controller per lamp

Using one ESP32 controller per lamp is recommended.

Several lamps can technically be declared on the same ESP32, but their commands cannot be advertised simultaneously. This can cause increased latency or interference between command sequences.

## Example with reliability settings

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

esphome:
  name: bedroom-lelight

external_components:
  - source: github://v1ack/lelight-esphome
    components:
      - ble_lelight

light:
  - platform: ble_lelight
    name: "Bedroom ceiling light"
    id: bedroom_ceiling_light
    lamp_mac: "01234567"
    interval: 300ms
    repeats: 3
    tx_power: 9dBm
```
