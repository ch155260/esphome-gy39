#include "gy39.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <cmath>

namespace esphome {
namespace gy39 {

static const char *const TAG = "gy39";

// UART protocol constants
static const uint8_t CMD_HEADER = 0xA5;
static const uint8_t CMD_QUERY_ALL = 0x53;     // query lux + BME280 once
static const uint8_t FRAME_HEADER = 0x5A;
static const uint8_t FRAME_TYPE_LUX = 0x15;    // 4 data bytes
static const uint8_t FRAME_TYPE_BME = 0x45;    // 10 data bytes
static const uint32_t RESPONSE_TIMEOUT_MS = 1000;

void GY39Component::dump_config() {
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Altitude", this->altitude_sensor_);
  LOG_SENSOR("  ", "Absolute Humidity", this->absolute_humidity_sensor_);
}

// ---------------------------------------------------------------- absolute humidity
//
// e   = RH/100 * f(p, t) * e_w(T)          actual vapour pressure (WMO definition of RH)
// rho = e * M_w / (Z * R * T)              vapour density
//
// e_w : saturation vapour pressure over liquid water
//       T >= 273.16 K : Wagner & Pruss (2002), IAPWS auxiliary equation (deviation from IAPWS-95 < 0.01 %)
//       T <  273.16 K : Murphy & Koop (2005), supercooled liquid water
//       Both give 611.657 Pa at the triple point, so the curve is continuous.
// f   : water-vapour enhancement factor in moist air, Buck (1996, CR-1A manual)
// Z   : compressibility factor of moist air, CIPM-2007 (Picard et al., Metrologia 45, 2008)

double GY39Component::saturation_vapor_pressure_water(double temperature_k) {
  const double T = temperature_k;
  if (T >= 273.16) {
    static const double TC = 647.096;    // K, critical temperature
    static const double PC = 22.064e6;   // Pa, critical pressure
    const double tau = 1.0 - T / TC;
    const double s = -7.85951783 * tau + 1.84408259 * std::pow(tau, 1.5) - 11.7866497 * std::pow(tau, 3.0) +
                     22.6807411 * std::pow(tau, 3.5) - 15.9618719 * std::pow(tau, 4.0) +
                     1.80122502 * std::pow(tau, 7.5);
    return PC * std::exp(TC / T * s);
  }
  const double ln_t = std::log(T);
  return std::exp(54.842763 - 6763.22 / T - 4.210 * ln_t + 0.000367 * T +
                  std::tanh(0.0415 * (T - 218.8)) * (53.878 - 1331.22 / T - 9.44523 * ln_t + 0.014025 * T));
}

double GY39Component::compute_absolute_humidity(double temperature_c, double rh_percent, double pressure_pa) {
  // Validity: BME280 operating range and range of the equations used.
  if (!std::isfinite(temperature_c) || !std::isfinite(rh_percent) || !std::isfinite(pressure_pa))
    return NAN;
  if (temperature_c < -45.0 || temperature_c > 85.0 || pressure_pa < 30000.0 || pressure_pa > 110000.0)
    return NAN;
  if (rh_percent < 0.0)
    rh_percent = 0.0;
  if (rh_percent > 100.0)
    rh_percent = 100.0;

  static const double R = 8.314462618;   // J/(mol·K), CODATA 2018
  static const double M_W = 18.01528e-3;  // kg/mol, molar mass of water

  const double t = temperature_c;
  const double T = t + 273.15;
  const double p_hpa = pressure_pa / 100.0;

  const double f = 1.0 + 1e-4 * (7.2 + p_hpa * (0.0320 + 5.9e-6 * t * t));
  const double e = rh_percent / 100.0 * f * saturation_vapor_pressure_water(T);
  const double xv = e / pressure_pa;  // mole fraction of water vapour

  const double p_t = pressure_pa / T;
  const double z = 1.0 -
                   p_t * (1.58123e-6 - 2.9331e-8 * t + 1.1043e-10 * t * t + (5.707e-6 - 2.051e-8 * t) * xv +
                          (1.9898e-4 - 2.376e-6 * t) * xv * xv) +
                   p_t * p_t * (1.83e-11 - 0.765e-8 * xv * xv);

  return e * M_W / (z * R * T) * 1000.0;  // kg/m³ -> g/m³
}

void GY39Component::publish_lux_(uint32_t raw) {
  float lux = raw / 100.0f;
  ESP_LOGD(TAG, "Illuminance: %.2f lx", lux);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(lux);
}

void GY39Component::publish_bme_(int16_t temp, uint32_t pressure, uint16_t humidity, int16_t altitude) {
  float t = temp / 100.0f;
  float p = pressure / 10000.0f;  // Pa*100 -> hPa
  float h = humidity / 100.0f;
  ESP_LOGD(TAG, "Temperature: %.2f°C, Pressure: %.2f hPa, Humidity: %.2f%%, Altitude: %d m", t, p, h, altitude);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(t);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(p);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(h);
  if (this->altitude_sensor_ != nullptr)
    this->altitude_sensor_->publish_state(altitude);

  if (this->absolute_humidity_sensor_ != nullptr) {
    double ah = compute_absolute_humidity(temp / 100.0, humidity / 100.0, pressure / 100.0);
    if (std::isnan(ah)) {
      ESP_LOGW(TAG, "Inputs out of range, absolute humidity not calculated");
      this->absolute_humidity_sensor_->publish_state(NAN);
    } else {
      ESP_LOGD(TAG, "Absolute humidity: %.3f g/m³", ah);
      this->absolute_humidity_sensor_->publish_state(static_cast<float>(ah));
    }
  }
}

// ---------------------------------------------------------------- UART
#ifdef GY39_USE_UART

void GY39UARTComponent::setup() {
  // Drop whatever the module already sent (it may be in continuous output mode).
  uint8_t dummy;
  while (this->available())
    this->read_byte(&dummy);
}

void GY39UARTComponent::send_command_(uint8_t cmd) {
  uint8_t data[3] = {CMD_HEADER, cmd, static_cast<uint8_t>(CMD_HEADER + cmd)};
  this->write_array(data, sizeof(data));
  this->flush();
}

void GY39UARTComponent::update() {
  if (this->want_lux_ || this->want_bme_) {
    ESP_LOGW(TAG, "No complete response to previous request%s%s", this->want_lux_ ? " (lux)" : "",
             this->want_bme_ ? " (BME280)" : "");
  }
  this->want_lux_ = this->illuminance_sensor_ != nullptr;
  this->want_bme_ = this->temperature_sensor_ != nullptr || this->pressure_sensor_ != nullptr ||
                    this->humidity_sensor_ != nullptr || this->altitude_sensor_ != nullptr ||
                    this->absolute_humidity_sensor_ != nullptr;
  this->last_request_ = millis();
  this->send_command_(CMD_QUERY_ALL);
}

void GY39UARTComponent::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;

    // Re-sync on the 0x5A 0x5A header.
    if (this->buffer_len_ < 2 && byte != FRAME_HEADER) {
      this->buffer_len_ = 0;
      continue;
    }
    this->buffer_[this->buffer_len_++] = byte;

    if (this->buffer_len_ == 3 && byte != FRAME_TYPE_LUX && byte != FRAME_TYPE_BME) {
      // "5A 5A 5A" -> keep the last two header bytes, otherwise restart.
      if (byte == FRAME_HEADER) {
        this->buffer_len_ = 2;
      } else {
        this->buffer_len_ = 0;
      }
      continue;
    }

    if (this->buffer_len_ >= 4) {
      uint8_t data_len = this->buffer_[3];
      uint8_t frame_len = 4 + data_len + 1;
      if (frame_len > MAX_FRAME_LEN) {
        this->buffer_len_ = 0;
        continue;
      }
      if (this->buffer_len_ == frame_len) {
        this->handle_frame_();
        this->buffer_len_ = 0;
      }
    }
  }

  if ((this->want_lux_ || this->want_bme_) && millis() - this->last_request_ > RESPONSE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Timeout waiting for GY-39 response");
    this->want_lux_ = this->want_bme_ = false;
    this->status_set_warning();
  }
}

void GY39UARTComponent::handle_frame_() {
  const uint8_t *b = this->buffer_;
  uint8_t len = this->buffer_len_;
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len - 1; i++)
    sum += b[i];
  if (sum != b[len - 1]) {
    ESP_LOGW(TAG, "Checksum mismatch: got 0x%02X, expected 0x%02X", b[len - 1], sum);
    return;
  }

  if (b[2] == FRAME_TYPE_LUX && b[3] == 4) {
    if (!this->want_lux_)
      return;  // continuous-output frame, only publish at update_interval
    this->want_lux_ = false;
    uint32_t raw = (uint32_t(b[4]) << 24) | (uint32_t(b[5]) << 16) | (uint32_t(b[6]) << 8) | b[7];
    this->publish_lux_(raw);
  } else if (b[2] == FRAME_TYPE_BME && b[3] == 10) {
    if (!this->want_bme_)
      return;
    this->want_bme_ = false;
    int16_t temp = int16_t((b[4] << 8) | b[5]);
    uint32_t pressure = (uint32_t(b[6]) << 24) | (uint32_t(b[7]) << 16) | (uint32_t(b[8]) << 8) | b[9];
    uint16_t humidity = (b[10] << 8) | b[11];
    int16_t altitude = int16_t((b[12] << 8) | b[13]);
    this->publish_bme_(temp, pressure, humidity, altitude);
  } else {
    ESP_LOGV(TAG, "Unknown frame type 0x%02X", b[2]);
    return;
  }
  this->status_clear_warning();
}

void GY39UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GY-39 (UART):");
  GY39Component::dump_config();
}

#endif  // GY39_USE_UART

// ---------------------------------------------------------------- I2C
#ifdef GY39_USE_I2C

void GY39I2CComponent::setup() {
  uint8_t data[2];
  if (this->read_register(0x00, data, sizeof(data)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with GY-39 failed (is S0 pulled to GND for I2C mode?)");
    this->mark_failed();
  }
}

void GY39I2CComponent::update() {
  uint8_t b[14];
  if (this->read_register(0x00, b, sizeof(b)) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Reading GY-39 registers failed");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  if (this->illuminance_sensor_ != nullptr) {
    uint32_t raw = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
    this->publish_lux_(raw);
  }
  int16_t temp = int16_t((b[4] << 8) | b[5]);
  uint32_t pressure = (uint32_t(b[6]) << 24) | (uint32_t(b[7]) << 16) | (uint32_t(b[8]) << 8) | b[9];
  uint16_t humidity = (b[10] << 8) | b[11];
  int16_t altitude = int16_t((b[12] << 8) | b[13]);
  this->publish_bme_(temp, pressure, humidity, altitude);
}

void GY39I2CComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GY-39 (I2C):");
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    ESP_LOGE(TAG, "Communication with GY-39 failed!");
  GY39Component::dump_config();
}

#endif  // GY39_USE_I2C

}  // namespace gy39
}  // namespace esphome
