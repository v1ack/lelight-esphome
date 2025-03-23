#pragma once

#include <vector>
#include <random>
#include <cstring>
#include <array>

namespace esphome {
namespace lelight_encode {

const std::array<int8_t, 16> BleLeABC = {-53, 106, -107, -115, -74, 123, 53, 90, 110, 73, 92, -123, 55, 60, -90, -120};

uint8_t checksum(const std::vector<uint8_t> &data);

std::vector<uint8_t> encode(uint8_t salt, const std::vector<uint8_t> &data);

struct BleLeCommand {
  uint8_t command;
  std::vector<uint8_t> value;
  BleLeCommand(uint8_t cmd, std::vector<uint8_t> val) : command(cmd), value(std::move(val)) {}
};

class BleLeEncoder {
 public:
  explicit BleLeEncoder(std::vector<uint8_t> mac) : message_id(0), mac(std::move(mac)) {}

  std::vector<uint8_t> mac;
  std::vector<uint8_t> message(const BleLeCommand &command);

 private:
  uint8_t message_id;
};

class BleLeCommands {
 public:
  static BleLeCommand turn_on() {
    std::vector<uint8_t> val;
    val.push_back(1);
    return BleLeCommand(0, val);
  }
  static BleLeCommand turn_off() {
    std::vector<uint8_t> val;
    val.push_back(-86);
    return BleLeCommand(1, val);
  }
  static BleLeCommand bright(float b) {
    uint32_t i = b * 1000;
    std::vector<uint8_t> val;
    val.push_back(static_cast<uint8_t>(i / 0xFF));
    val.push_back(static_cast<uint8_t>(i % 0xFF));
    return BleLeCommand(8, val);
  }
  static BleLeCommand temp(float t) {
    uint32_t i = 3000 + (1 - t) * (6400 - 3000);
    std::vector<uint8_t> val;
    if (i > 4700) {
      val.push_back(-1);
      val.push_back(static_cast<uint8_t>(((6400 - i) * 128) / 1700.0));
      return BleLeCommand(13, val);
    }

    val.push_back(static_cast<uint8_t>(((i - 3000) * 128) / 1700.0));
    val.push_back(-1);
    return BleLeCommand(13, val);
  }
};
}  // namespace lelight_encode
}  // namespace esphome