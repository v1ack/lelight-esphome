#pragma once

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/transformers.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include <esp_bt.h>
#include <esp_gap_ble_api.h>

#include "encoder.h"

namespace esphome {
namespace ble_lelight {

using namespace esp32_ble;
using namespace lelight_encode;

class NoTransitionLightTransformer : public light::LightTransformer {
 public:
  bool is_finished() override { return true; }
  optional<light::LightColorValues> apply() override {
    return light::LightColorValues(this->target_values_);
  }
};

class BleLeLight : public light::LightOutput,
                   public Component,
                   public Parented<ESP32BLE> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(156.25f);
    traits.set_max_mireds(333.33f);
    return traits;
  }

  void write_state(light::LightState *state) override;

  void set_interval(uint16_t value) { this->advertise_duration_ms_ = value; }
  void set_tx_power(esp_power_level_t value) { this->tx_power_ = value; }
  void set_repeats(uint8_t value) { this->repeats_ = value > 0 ? value : 1; }
  void set_encoder(const std::string &hex);

  std::unique_ptr<light::LightTransformer> create_default_transition() override {
    return make_unique<NoTransitionLightTransformer>();
  }

 protected:
  bool start_current_packet_();
  void prepare_next_packet_();
  void replace_pending_state_(bool is_on, float color_temperature, float brightness);

  std::deque<BleLeCommand> pending_commands_;
  std::vector<uint8_t> current_packet_;

  BleLeEncoder encoder_{};

  uint32_t phase_started_at_{0};
  uint16_t advertise_duration_ms_{250};
  uint16_t command_gap_ms_{75};
  esp_power_level_t tx_power_{};
  esp_ble_adv_params_t ble_adv_params_{};

  uint8_t repeats_{2};
  uint8_t repeats_left_{0};
  bool advertising_{false};
  bool packet_ready_{false};
};

}  // namespace ble_lelight
}  // namespace esphome

#endif
