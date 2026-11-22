import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, text_sensor, uart
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_LUX,
    UNIT_METER,
    UNIT_PERCENT,
)

CONF_ALTITUDE = "altitude"
CONF_ABSOLUTE_HUMIDITY = "absolute_humidity"
CONF_DEW_POINT = "dew_point"
# Literal values so older ESPHome releases without these constants still work.
DEVICE_CLASS_ABSOLUTE_HUMIDITY = "absolute_humidity"
UNIT_GRAMS_PER_CUBIC_METER = "g/m³"
CONF_INTERFACE = "interface"
CONF_MIXING_RATIO = "mixing_ratio"
CONF_SEA_LEVEL_PRESSURE = "sea_level_pressure"
CONF_STATION_ALTITUDE = "station_altitude"
CONF_PRESSURE_TREND_3H = "pressure_trend_3h"
CONF_PRESSURE_TREND_24H = "pressure_trend_24h"
CONF_PRESSURE_TENDENCY = "pressure_tendency"
CONF_TENDENCY_LANGUAGE = "tendency_language"
UNIT_GRAMS_PER_KILOGRAM = "g/kg"
CONF_TEMPERATURE_OFFSET = "temperature_offset"
CONF_HUMIDITY_CALIBRATION = "humidity_calibration"
CONF_PRESSURE_OFFSET = "pressure_offset"


def _calibration_point(value):
    """'measured -> true' (same syntax as ESPHome's calibrate_linear)."""
    value = cv.string_strict(value)
    parts = value.split("->")
    if len(parts) != 2:
        raise cv.Invalid("Calibration point must be in the form 'measured -> true', e.g. '77.0 -> 75.3'")
    measured = cv.float_(parts[0].strip())
    true = cv.float_(parts[1].strip())
    return (measured, true)


def _humidity_calibration(value):
    points = cv.ensure_list(_calibration_point)(value)
    if not 1 <= len(points) <= 2:
        raise cv.Invalid("Give one point (offset only) or two points (gain and offset)")
    if len(points) == 2 and abs(points[0][0] - points[1][0]) < 5:
        raise cv.Invalid("The two measured values must be at least 5 %RH apart")
    return points

AUTO_LOAD = ["text_sensor"]

gy39_ns = cg.esphome_ns.namespace("gy39")
GY39Component = gy39_ns.class_("GY39Component", cg.PollingComponent)
GY39UARTComponent = gy39_ns.class_("GY39UARTComponent", GY39Component, uart.UARTDevice)
GY39I2CComponent = gy39_ns.class_("GY39I2CComponent", GY39Component, i2c.I2CDevice)

BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            icon="mdi:altimeter",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ABSOLUTE_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_GRAMS_PER_CUBIC_METER,
            icon="mdi:water",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ABSOLUTE_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DEW_POINT): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:thermometer-water",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MIXING_RATIO): sensor.sensor_schema(
            unit_of_measurement=UNIT_GRAMS_PER_KILOGRAM,
            icon="mdi:water-percent",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SEA_LEVEL_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_STATION_ALTITUDE): cv.float_range(min=-500, max=9000),
        cv.Optional(CONF_PRESSURE_TREND_3H): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            icon="mdi:chart-line-variant",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRESSURE_TREND_24H): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            icon="mdi:chart-line-variant",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRESSURE_TENDENCY): text_sensor.text_sensor_schema(
            icon="mdi:weather-partly-cloudy",
        ),
        cv.Optional(CONF_TEMPERATURE_OFFSET, default=0.0): cv.float_range(min=-10, max=10),
        cv.Optional(CONF_HUMIDITY_CALIBRATION): _humidity_calibration,
        cv.Optional(CONF_PRESSURE_OFFSET, default=0.0): cv.float_range(min=-20, max=20),
        cv.Optional(CONF_TENDENCY_LANGUAGE, default="en"): cv.one_of("en", "zh-TW", lower=False),
    }
).extend(cv.polling_component_schema("60s"))


def _validate_station_altitude(config):
    if CONF_SEA_LEVEL_PRESSURE in config and CONF_STATION_ALTITUDE not in config:
        raise cv.Invalid(
            f"'{CONF_STATION_ALTITUDE}' (height of the sensor above mean sea level, in m) "
            f"is required for '{CONF_SEA_LEVEL_PRESSURE}'"
        )
    return config

CONFIG_SCHEMA = cv.All(cv.typed_schema(
    {
        "uart": BASE_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(GY39UARTComponent)}
        ).extend(uart.UART_DEVICE_SCHEMA),
        "i2c": BASE_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(GY39I2CComponent)}
        ).extend(i2c.i2c_device_schema(0x5B)),
    },
    key=CONF_INTERFACE,
    default_type="uart",
    lower=True,
), _validate_station_altitude)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_INTERFACE] == "uart":
        cg.add_define("GY39_USE_UART")
        await uart.register_uart_device(var, config)
    else:
        cg.add_define("GY39_USE_I2C")
        await i2c.register_i2c_device(var, config)

    for key, setter in (
        (CONF_ILLUMINANCE, "set_illuminance_sensor"),
        (CONF_TEMPERATURE, "set_temperature_sensor"),
        (CONF_PRESSURE, "set_pressure_sensor"),
        (CONF_HUMIDITY, "set_humidity_sensor"),
        (CONF_ALTITUDE, "set_altitude_sensor"),
        (CONF_ABSOLUTE_HUMIDITY, "set_absolute_humidity_sensor"),
        (CONF_DEW_POINT, "set_dew_point_sensor"),
        (CONF_MIXING_RATIO, "set_mixing_ratio_sensor"),
        (CONF_SEA_LEVEL_PRESSURE, "set_sea_level_pressure_sensor"),
        (CONF_PRESSURE_TREND_3H, "set_pressure_trend_3h_sensor"),
        (CONF_PRESSURE_TREND_24H, "set_pressure_trend_24h_sensor"),
    ):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))

    if CONF_PRESSURE_TENDENCY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PRESSURE_TENDENCY])
        cg.add(var.set_pressure_tendency_text_sensor(sens))
    if CONF_STATION_ALTITUDE in config:
        cg.add(var.set_station_altitude(config[CONF_STATION_ALTITUDE]))
    cg.add(var.set_tendency_language_zh(config[CONF_TENDENCY_LANGUAGE] == "zh-TW"))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(var.set_pressure_offset(config[CONF_PRESSURE_OFFSET]))
    if CONF_HUMIDITY_CALIBRATION in config:
        pts = config[CONF_HUMIDITY_CALIBRATION]
        if len(pts) == 1:
            gain, offset = 1.0, pts[0][1] - pts[0][0]
        else:
            (x1, y1), (x2, y2) = pts
            gain = (y2 - y1) / (x2 - x1)
            offset = y1 - gain * x1
        cg.add(var.set_humidity_calibration(gain, offset))
