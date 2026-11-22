import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, uart
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
# Literal values so older ESPHome releases without these constants still work.
DEVICE_CLASS_ABSOLUTE_HUMIDITY = "absolute_humidity"
UNIT_GRAMS_PER_CUBIC_METER = "g/m³"
CONF_INTERFACE = "interface"

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
    }
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.typed_schema(
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
)


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
    ):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))
