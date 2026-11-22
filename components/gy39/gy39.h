#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#ifdef GY39_USE_UART
#include "esphome/components/uart/uart.h"
#endif
#ifdef GY39_USE_I2C
#include "esphome/components/i2c/i2c.h"
#endif

namespace esphome {
namespace gy39 {

/// Common part: holds the sensors and converts raw values.
class GY39Component : public PollingComponent {
 public:
  void set_illuminance_sensor(sensor::Sensor *s) { this->illuminance_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_pressure_sensor(sensor::Sensor *s) { this->pressure_sensor_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sensor_ = s; }
  void set_altitude_sensor(sensor::Sensor *s) { this->altitude_sensor_ = s; }
  void set_absolute_humidity_sensor(sensor::Sensor *s) { this->absolute_humidity_sensor_ = s; }

  /// Absolute humidity (water vapour mass concentration) in g/m³.
  /// @param temperature_c  air temperature in °C
  /// @param rh_percent     relative humidity over liquid water in % (WMO definition)
  /// @param pressure_pa    total air pressure in Pa
  /// Returns NAN if the inputs are outside the valid range.
  static double compute_absolute_humidity(double temperature_c, double rh_percent, double pressure_pa);
  /// Saturation vapour pressure over (supercooled) liquid water in Pa, T in K.
  static double saturation_vapor_pressure_water(double temperature_k);

  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  /// raw = lux * 100 (big endian, 4 bytes)
  void publish_lux_(uint32_t raw);
  /// temp: °C*100 (signed), pressure: Pa*100, humidity: %*100, altitude: m (signed)
  void publish_bme_(int16_t temp, uint32_t pressure, uint16_t humidity, int16_t altitude);

  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *altitude_sensor_{nullptr};
  sensor::Sensor *absolute_humidity_sensor_{nullptr};
};

#ifdef GY39_USE_UART
/// UART mode (factory default, 9600 8N1). Uses the "query once" commands.
class GY39UARTComponent : public GY39Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  void send_command_(uint8_t cmd);
  void handle_frame_();

  static const uint8_t MAX_FRAME_LEN = 15;
  uint8_t buffer_[MAX_FRAME_LEN];
  uint8_t buffer_len_{0};
  bool want_lux_{false};
  bool want_bme_{false};
  uint32_t last_request_{0};
};
#endif

#ifdef GY39_USE_I2C
/// I2C mode (pull S0 to GND before power-up). Address 0x5B, registers 0x00..0x0D.
class GY39I2CComponent : public GY39Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
};
#endif

}  // namespace gy39
}  // namespace esphome
