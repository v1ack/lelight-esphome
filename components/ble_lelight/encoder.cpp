#include "encoder.h"

#include <algorithm>
#include <cstring>

#include <esp_random.h>

namespace esphome {
namespace lelight_encode {

uint8_t checksum(const std::vector<uint8_t> &data) {
  uint8_t sum = 0;
  for (const uint8_t byte : data)
    sum = static_cast<uint8_t>(sum + byte);
  return static_cast<uint8_t>(~sum);
}

std::vector<uint8_t> encode(uint8_t salt, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> result(data);
  const uint8_t key = BLE_LE_ABC[salt & 0x0F];

  for (size_t i = 10; i < result.size(); i++)
    result[i] ^= key;

  return result;
}

uint8_t BleLeEncoder::next_message_id_() {
  // Zero is skipped because the original implementation started at one.
  // uint8_t wraparound itself is well-defined and does not corrupt memory.
  this->message_id_++;
  if (this->message_id_ == 0)
    this->message_id_ = 1;
  return this->message_id_;
}

std::vector<uint8_t> BleLeEncoder::message(const BleLeCommand &command) {
  std::vector<uint8_t> payload(command.value.size() + 11, 0);
  payload[0] = 0x01;
  std::copy(this->lamp_id_.begin(), this->lamp_id_.end(), payload.begin() + 1);
  payload[5] = 0xFE;
  payload[6] = 0x00;
  payload[7] = this->next_message_id_();
  payload[8] = command.command;
  payload[9] = static_cast<uint8_t>(command.value.size());
  std::copy(command.value.begin(), command.value.end(), payload.begin() + 10);
  payload.back() = checksum(std::vector<uint8_t>(payload.begin(), payload.end() - 1));

  std::vector<uint8_t> packet(payload.size() + 5, 0);
  const uint32_t random_prefix = esp_random();
  std::memcpy(packet.data(), &random_prefix, sizeof(random_prefix));
  packet[4] = static_cast<uint8_t>(payload.size());
  std::copy(payload.begin(), payload.end(), packet.begin() + 5);

  return encode(this->lamp_id_[0], packet);
}

}  // namespace lelight_encode
}  // namespace esphome
