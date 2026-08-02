#include "ble_lelight.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>

namespace esphome {
namespace ble_lelight {

static const char *const TAG = "ble_lelight";

static std::string command_to_string(const BleLeCommand &command) {
  std::string result = "Command{command=" + std::to_string(command.command) + ", value=[";
  for (size_t i = 0; i < command.value.size(); ++i) {
    result += std::to_string(command.value[i]);
    if (i + 1 < command.value.size())
      result += ", ";
  }
  result += "]}";
  return result;
}

void BleLeLight::dump_config() {
  const auto &id = this->encoder_.lamp_id();
  ESP_LOGCONFIG(TAG, "BLE LeLight output:");
  ESP_LOGCONFIG(TAG, "  Lamp ID: %02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
  ESP_LOGCONFIG(TAG, "  Advertisement duration: %u ms", this->advertise_duration_ms_);
  ESP_LOGCONFIG(TAG, "  Repeats per command: %u", this->repeats_);
}

float BleLeLight::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BleLeLight::setup() {
  this->ble_adv_params_ = {
      .adv_int_min = 0x20,
      .adv_int_max = 0x40,
      .adv_type = ADV_TYPE_NONCONN_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };
}

void BleLeLight::prepare_next_packet_() {
  if (this->packet_ready_ || this->pending_commands_.empty())
    return;

  const auto command = this->pending_commands_.front();
  this->pending_commands_.pop_front();

  // Encode only once. All retries advertise exactly the same physical packet,
  // so message_id and random prefix do not change between retries.
  this->current_packet_ = this->encoder_.message(command);
  this->repeats_left_ = this->repeats_;
  this->packet_ready_ = true;

  ESP_LOGD(TAG, "Prepared %s (%u repeats)", command_to_string(command).c_str(), this->repeats_left_);
}

bool BleLeLight::start_current_packet_() {
  if (!this->packet_ready_ || this->current_packet_.empty())
    return false;

  esp_ble_adv_data_t adv_data = {
      .set_scan_rsp = false,
      .include_name = false,
      .include_txpower = false,
      .min_interval = 0x20,
      .max_interval = 0x40,
      .appearance = 0x00,
      .manufacturer_len = static_cast<uint16_t>(this->current_packet_.size()),
      .p_manufacturer_data = this->current_packet_.data(),
      .service_data_len = 0,
      .p_service_data = nullptr,
      .service_uuid_len = 0,
      .p_service_uuid = nullptr,
      .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
  };

  esp_err_t err = esp_ble_gap_config_adv_data(&adv_data);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_config_adv_data failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, this->tx_power_);
  if (err != ESP_OK)
    ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));

  err = esp_ble_gap_start_advertising(&this->ble_adv_params_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
    return false;
  }

  this->advertising_ = true;
  this->phase_started_at_ = millis();
  ESP_LOGD(TAG, "Advertising started, %u attempts left including this one", this->repeats_left_);
  return true;
}

void BleLeLight::loop() {
  const uint32_t now = millis();

  if (this->advertising_) {
    if (now - this->phase_started_at_ < this->advertise_duration_ms_)
      return;

    const esp_err_t err = esp_ble_gap_stop_advertising();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: %s", esp_err_to_name(err));
      return;
    }

    this->advertising_ = false;
    this->phase_started_at_ = now;

    if (this->repeats_left_ > 0)
      this->repeats_left_--;

    if (this->repeats_left_ == 0) {
      this->packet_ready_ = false;
      this->current_packet_.clear();
    }
    return;
  }

  if (now - this->phase_started_at_ < this->command_gap_ms_)
    return;

  this->prepare_next_packet_();
  if (!this->packet_ready_)
    return;

  // On a synchronous failure the packet is retained and retried on the next
  // loop after command_gap_ms_. No command is silently lost.
  if (!this->start_current_packet_())
    this->phase_started_at_ = now;
}

void BleLeLight::replace_pending_state_(bool is_on, float color_temperature, float brightness) {
  // Keep the packet currently on air, but collapse every not-yet-sent update
  // into the newest desired state. Slider movements therefore cannot be lost.
  this->pending_commands_.clear();
  this->pending_commands_.push_back(is_on ? BleLeCommands::turn_on() : BleLeCommands::turn_off());

  if (is_on) {
    this->pending_commands_.push_back(BleLeCommands::temp(color_temperature));
    this->pending_commands_.push_back(BleLeCommands::bright(brightness));
  }
}

void BleLeLight::write_state(light::LightState *state) {
  float color_temperature = 0.0f;
  float brightness = 0.0f;
  state->current_values_as_ct(&color_temperature, &brightness);

  const bool is_on = state->current_values.is_on();
  this->replace_pending_state_(is_on, color_temperature, brightness);

  ESP_LOGD(TAG, "Queued latest state: temperature=%f brightness=%f on=%d", color_temperature, brightness, is_on);
}

void BleLeLight::set_encoder(const std::string &hex) {
  if (hex.length() != 8) {
    ESP_LOGE(TAG, "Lamp ID must contain exactly 8 hex characters");
    return;
  }

  std::array<uint8_t, 4> bytes{};
  for (size_t i = 0; i < bytes.size(); i++) {
    const std::string byte = hex.substr(i * 2, 2);
    char *end = nullptr;
    const unsigned long value = std::strtoul(byte.c_str(), &end, 16);
    if (end == byte.c_str() || *end != '\0' || value > 0xFF) {
      ESP_LOGE(TAG, "Invalid lamp ID: %s", hex.c_str());
      return;
    }
    bytes[i] = static_cast<uint8_t>(value);
  }

  this->encoder_ = BleLeEncoder(bytes);
}

}  // namespace ble_lelight
}  // namespace esphome

#endif
