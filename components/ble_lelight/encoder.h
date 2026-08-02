#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace esphome {
namespace lelight_encode {

constexpr std::array<uint8_t, 16> BLE_LE_ABC = {
    0xCB, 0x6A, 0x95, 0x8D, 0xB6, 0x7B, 0x35, 0x5A,
    0x6E, 0x49, 0x5C, 0x85, 0x37, 0x3C, 0xA6, 0x88,
};

uint8_t checksum(const std::vector<uint8_t> &data);
std::vector<uint8_t> encode(uint8_t salt, const std::vector<uint8_t> &data);

struct BleLeCommand {
  uint8_t command;
  std::vector<uint8_t> value;

  BleLeCommand(uint8_t command, std::vector<uint8_t> value)
      : command(command), value(std::move(value)) {}
};

class BleLeEncoder {
 public:
  explicit BleLeEncoder(std::array<uint8_t, 4> lamp_id = {}) : lamp_id_(lamp_id) {}

  const std::array<uint8_t, 4> &lamp_id() const { return this->lamp_id_; }
  std::vector<uint8_t> message(const BleLeCommand &command);

 private:
  uint8_t next_message_id_();

  uint8_t message_id_{0};
  std::array<uint8_t, 4> lamp_id_{};
};

class BleLeCommands {
 public:
  static BleLeCommand turn_on() { return BleLeCommand(0x00, {0x01}); }
  static BleLeCommand turn_off() { return BleLeCommand(0x01, {0xAA}); }

  static BleLeCommand bright(float brightness) {
    if (brightness < 0.0f)
      brightness = 0.0f;
    if (brightness > 1.0f)
      brightness = 1.0f;

    // Kept compatible with the captured protocol implementation. This is
    // intentionally base-255, not a normal uint16 big-endian split.
    const uint32_t value = static_cast<uint32_t>(brightness * 1000.0f);
    return BleLeCommand(0x08,
                        {static_cast<uint8_t>(value / 0xFF),
                         static_cast<uint8_t>(value % 0xFF)});
  }

  static BleLeCommand temp(float temperature) {
    if (temperature < 0.0f)
      temperature = 0.0f;
    if (temperature > 1.0f)
      temperature = 1.0f;

    const uint32_t kelvin =
        static_cast<uint32_t>(3000.0f + (1.0f - temperature) * (6400.0f - 3000.0f));

    if (kelvin > 4700) {
      return BleLeCommand(
          0x0D,
          {0xFF, static_cast<uint8_t>(((6400 - kelvin) * 128) / 1700.0f)});
    }

    return BleLeCommand(
        0x0D,
        {static_cast<uint8_t>(((kelvin - 3000) * 128) / 1700.0f), 0xFF});
  }
};

}  // namespace lelight_encode
}  // namespace esphome
