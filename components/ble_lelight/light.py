import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble, light
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import CONF_BLE_ID
from esphome.const import CONF_OUTPUT_ID, CONF_TX_POWER
from esphome.core import TimePeriod

AUTO_LOAD = ["esp32_ble"]
DEPENDENCIES = ["esp32"]

ble_lelight = cg.esphome_ns.namespace("ble_lelight")
BleLeLight = ble_lelight.class_(
    "BleLeLight",
    light.LightOutput,
    cg.Component,
    cg.Parented.template(esp32_ble.ESP32BLE),
)

CONF_INTERVAL = "interval"
CONF_LAMP_MAC = "lamp_mac"
CONF_REPEATS = "repeats"


def light_id_str(value: str):
    value = cv.string_strict(value).upper()
    if len(value) != 8:
        raise cv.Invalid("Lamp ID must consist of exactly 8 hexadecimal characters")

    try:
        int(value, 16)
    except ValueError as exc:
        raise cv.Invalid("Lamp ID must consist of exactly 8 hexadecimal characters") from exc

    return value


CONFIG_SCHEMA = light.LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BleLeLight),
        cv.GenerateID(CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Required(CONF_LAMP_MAC): light_id_str,
        cv.Optional(CONF_INTERVAL, default="250ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(
                min=TimePeriod(milliseconds=200),
                max=TimePeriod(milliseconds=1000),
            ),
        ),
        cv.Optional(CONF_REPEATS, default=2): cv.int_range(min=1, max=5),
        cv.Optional(CONF_TX_POWER, default="9dBm"): cv.All(
            cv.decibel,
            cv.enum(esp32_ble.TX_POWER_LEVELS, int=True),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])

    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_BLE_ID])
    await light.register_light(var, config)

    cg.add(var.set_encoder(config[CONF_LAMP_MAC]))
    cg.add(var.set_interval(config[CONF_INTERVAL]))
    cg.add(var.set_repeats(config[CONF_REPEATS]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))

    # CORE.using_esp_idf was removed in ESPHome 2026.7. These options are safe
    # for ESP32 builds and match the requirements of the low-level GAP API.
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)
