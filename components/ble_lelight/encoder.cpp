#include <vector>
#include <random>
#include <cstring>

#include "encoder.h"

namespace esphome {
namespace lelight_encode {

uint8_t checksum(const std::vector<uint8_t> &data) {
  uint8_t sum = 0;
  for (uint8_t b : data) {
    sum += b;
  }
  return ~sum;
}

std::vector<uint8_t> encode(uint8_t salt, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> res(data.size(), 0);
  for (size_t i = 0; i < data.size(); i++) {
    res[i] = (i < 10) ? data[i] : static_cast<uint8_t>(data[i] ^ BleLeABC[salt & 15]);
  }
  return res;
}

std::vector<uint8_t> BleLeEncoder::message(const BleLeCommand &command) {
  message_id++;
  if (message_id == 120)
    message_id = 1;

  std::vector<uint8_t> data(command.value.size() + 11, 0);
  data[0] = 1;
  data[1] = mac[0];
  data[2] = mac[1];
  data[3] = mac[2];
  data[4] = mac[3];
  data[5] = 254;
  data[6] = 0;
  data[7] = message_id;
  data[8] = command.command;
  data[9] = command.value.size();

  copy(command.value.begin(), command.value.end(), data.begin() + 10);
  data[10 + command.value.size()] =
      checksum(std::vector<uint8_t>(data.begin(), data.begin() + command.value.size() + 10));

  std::vector<uint8_t> data2(data.size() + 5, 0);
  for (int i = 0; i < 4; i++) {
    data2[i] = static_cast<uint8_t>(rand() % 256);
    // data2[i] = 0;
  }
  data2[4] = data.size();
  copy(data.begin(), data.end(), data2.begin() + 5);

  return encode(mac[0], data2);
}

}  // namespace lelight_encode
}  // namespace esphome
