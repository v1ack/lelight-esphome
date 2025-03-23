#include "ble_lelight.h"
#include "esphome/core/log.h"
#include "esphome/components/light/light_output.h"

#ifdef USE_ESP32

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "encoder.h"

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

namespace esphome {
namespace ble_lelight {

using namespace lelight_encode;

std::string commandToString(const BleLeCommand &cmd) {
  std::string result = "Command{command=" + std::to_string(cmd.command) + ", value=[";
  for (size_t i = 0; i < cmd.value.size(); ++i) {
    result += std::to_string(cmd.value[i]);
    if (i < cmd.value.size() - 1) {
      result += ", ";
    }
  }
  result += "]}";
  return result;
}

static const char *const TAG = "ble_lelight";

void BleLeLight::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE LeLight output:");
  char mac[4];
  char *bpos = mac;
  for (int8_t ii = 0; ii < 4; ++ii) {
    bpos += sprintf(bpos, "%02X", this->encoder.mac[ii]);
  }
  ESP_LOGCONFIG(TAG, "  Min Interval: %ums, TX Power: %ddBm, Lamp ID: %s", this->interval_, (this->tx_power_ * 3) - 12,
                mac);
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
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST,
  };
}

void BleLeLight::loop() {
  const uint32_t now = millis();
  if (now - this->last_advertisement_time_ < this->interval_) {
    return;
  }
  esp_err_t err;
  if (this->advertising_) {
    ESP_LOGD(TAG, "Stopping advertising");
    err = esp_ble_gap_stop_advertising();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: %s", esp_err_to_name(err));
    }
    this->advertising_ = false;
    this->last_advertisement_time_ = now - this->interval_ / 2;
    return;
  }
  if (this->lamp_commands_queue.empty()) {
    return;
  }
  auto command = this->lamp_commands_queue.front();
  this->lamp_commands_queue.pop();
  this->last_advertisement_time_ = now;

  std::vector<uint8_t> manufacturer_data = this->encoder.message(command);

  esp_ble_adv_data_t adv_data = {
      .set_scan_rsp = false,
      .include_name = true,
      .include_txpower = false,
      .min_interval = 0x20,  // ~20 мс между пакетами
      .max_interval = 0x40,  // ~40 мс между пакетами
      .appearance = 0x00,
      .manufacturer_len = static_cast<uint16_t>(manufacturer_data.size()),  // Длина вектора
      .p_manufacturer_data = manufacturer_data.data(),
      .service_data_len = 0,
      .p_service_data = NULL,
      .service_uuid_len = 0,
      .p_service_uuid = NULL,
      .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
  };
  err = esp_ble_gap_config_adv_data(&adv_data);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_config_adv_data failed: %s", esp_err_to_name(err));
  }
  err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, this->tx_power_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_start_advertising(&this->ble_adv_params_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
  }
  this->advertising_ = true;

  ESP_LOGD(TAG, "Advertising started. %s", commandToString(command).c_str());
}

void BleLeLight::write_state(light::LightState *state) {
  float color_temperature = 0.0f, brightness = 0.0f;
  state->current_values_as_ct(&color_temperature, &brightness);

  bool is_on = state->current_values.is_on();
  this->lamp_commands_queue.push(is_on ? BleLeCommands::turn_on() : BleLeCommands::turn_off());

  if (is_on) {
    this->lamp_commands_queue.push(BleLeCommands::temp(color_temperature));
    this->lamp_commands_queue.push(BleLeCommands::bright(brightness));
  }

  ESP_LOGD(TAG, "Setting BLE advertising data %f %f %d", color_temperature, brightness, state->current_values.is_on());
}

void BleLeLight::set_encoder(const std::string &hex) {
  if (hex.length() % 2 != 0) {
    ESP_LOGW(TAG, "Invalid hex string length: %d", hex.length());
    return;
  }

  std::vector<uint8_t> bytes;
  bytes.reserve(hex.length() / 2);

  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    bytes.push_back(byte);
    this->encoder = BleLeEncoder(bytes);
  }
}

}  // namespace ble_lelight
}  // namespace esphome

#endif
