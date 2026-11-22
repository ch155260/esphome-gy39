#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

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
  void set_dew_point_sensor(sensor::Sensor *s) { this->dew_point_sensor_ = s; }
  void set_mixing_ratio_sensor(sensor::Sensor *s) { this->mixing_ratio_sensor_ = s; }
  void set_sea_level_pressure_sensor(sensor::Sensor *s) { this->sea_level_pressure_sensor_ = s; }
  void set_pressure_trend_3h_sensor(sensor::Sensor *s) { this->pressure_trend_3h_sensor_ = s; }
  void set_pressure_trend_24h_sensor(sensor::Sensor *s) { this->pressure_trend_24h_sensor_ = s; }
  void set_pressure_tendency_text_sensor(text_sensor::TextSensor *s) { this->pressure_tendency_text_sensor_ = s; }
  void set_station_altitude(float h) { this->station_altitude_ = h; }
  void set_tendency_language_zh(bool zh) { this->tendency_zh_ = zh; }
  void set_temperature_offset(float o) { this->temperature_offset_ = o; }
  void set_humidity_calibration(float gain, float offset) {
    this->humidity_gain_ = gain;
    this->humidity_offset_ = offset;
  }
  void set_pressure_offset(float o_hpa) { this->pressure_offset_ = o_hpa; }

  /// Apply calibration. RH is calibrated first (at the sensor's own temperature); the temperature
  /// offset is then applied and RH re-evaluated at the corrected temperature with the vapour
  /// pressure kept constant (self-heating does not change the water content of the air).
  static void apply_calibration(double t_raw, double rh_raw, double p_pa, double t_offset, double rh_gain,
                                double rh_offset, double *t_out, double *rh_out);

  /// Mixing ratio (mass of water vapour per mass of dry air) in g/kg.
  static double compute_mixing_ratio(double temperature_c, double rh_percent, double pressure_pa);
  /// Station pressure reduced to mean sea level in Pa (DWD reduction formula).
  static double compute_sea_level_pressure(double temperature_c, double rh_percent, double pressure_pa,
                                           double station_altitude_m);
  /// WMO / Met Office tendency class for a 3 h change in hPa: -5..5 (0 = steady).
  static int tendency_class(double change_3h_hpa);

  /// Absolute humidity (water vapour mass concentration) in g/m³.
  /// @param temperature_c  air temperature in °C
  /// @param rh_percent     relative humidity over liquid water in % (WMO definition)
  /// @param pressure_pa    total air pressure in Pa
  /// Returns NAN if the inputs are outside the valid range.
  static double compute_absolute_humidity(double temperature_c, double rh_percent, double pressure_pa);
  /// Dew point over liquid water in °C (NAN if out of range or RH = 0).
  static double compute_dew_point(double temperature_c, double rh_percent, double pressure_pa);
  /// Actual water vapour pressure in Pa (NAN if inputs are out of range).
  static double vapor_pressure(double temperature_c, double rh_percent, double pressure_pa);
  /// Water vapour enhancement factor f(p, t) in moist air (Buck 1996).
  static double enhancement_factor(double temperature_c, double pressure_pa);
  /// Saturation vapour pressure over (supercooled) liquid water in Pa, T in K.
  static double saturation_vapor_pressure_water(double temperature_k);

  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  /// raw = lux * 100 (big endian, 4 bytes)
  void publish_lux_(uint32_t raw);
  /// temp: °C*100 (signed), pressure: Pa*100, humidity: %*100, altitude: m (signed)
  void publish_bme_(int16_t temp, uint32_t pressure, uint16_t humidity, int16_t altitude);
  /// Feed one station-pressure reading (Pa) into the 10-minute history.
  void update_pressure_history_(double pressure_pa);
  void publish_trends_();
  float history_ago_(uint16_t bins) const;

  /// Pressure history: 10-minute means, 24 h + 1 bins (needed for the 24 h difference).
  static const uint32_t BIN_MS = 10UL * 60UL * 1000UL;
  static const uint16_t HISTORY_BINS = 24 * 6 + 1;
  float history_[HISTORY_BINS];
  uint16_t history_head_{0};
  uint16_t history_count_{0};
  double bin_sum_{0.0};
  uint16_t bin_n_{0};
  uint32_t bin_start_{0};
  bool bin_started_{false};

  float station_altitude_{NAN};
  float temperature_offset_{0.0f};
  float humidity_gain_{1.0f};
  float humidity_offset_{0.0f};
  float pressure_offset_{0.0f};
  bool tendency_zh_{false};

  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *altitude_sensor_{nullptr};
  sensor::Sensor *absolute_humidity_sensor_{nullptr};
  sensor::Sensor *dew_point_sensor_{nullptr};
  sensor::Sensor *mixing_ratio_sensor_{nullptr};
  sensor::Sensor *sea_level_pressure_sensor_{nullptr};
  sensor::Sensor *pressure_trend_3h_sensor_{nullptr};
  sensor::Sensor *pressure_trend_24h_sensor_{nullptr};
  text_sensor::TextSensor *pressure_tendency_text_sensor_{nullptr};
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
