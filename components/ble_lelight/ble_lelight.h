#pragma once

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <esp_bt.h>
#include <esp_gap_ble_api.h>

#include "encoder.h"

namespace esphome {
namespace ble_lelight {

using namespace esp32_ble;
using namespace lelight_encode;

struct LeLampCommand {
  uint8_t command;
  float value;
};

class BleLeLight : public light::LightOutput, public Component, public Parented<ESP32BLE> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(156.25);
    traits.set_max_mireds(333.33);
    return traits;
  }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

  void set_interval(uint16_t val) { this->interval_ = val; }
  void set_tx_power(esp_power_level_t val) { this->tx_power_ = val; }
  void set_encoder(const std::string &hex);

 protected:
  light::LightState *state_{nullptr};

  std::queue<BleLeCommand> lamp_commands_queue;
  u_int32_t last_advertisement_time_{0};

  BleLeEncoder encoder{{0, 0, 0, 0}};

  uint16_t interval_{};
  esp_power_level_t tx_power_{};
  esp_ble_adv_params_t ble_adv_params_;
  bool advertising_{false};
};

}  // namespace ble_lelight
}  // namespace esphome

#endif
