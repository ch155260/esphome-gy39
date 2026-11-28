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
  LOG_SENSOR("  ", "Dew Point", this->dew_point_sensor_);
  LOG_SENSOR("  ", "Mixing Ratio", this->mixing_ratio_sensor_);
  LOG_SENSOR("  ", "Sea Level Pressure", this->sea_level_pressure_sensor_);
  if (this->sea_level_pressure_sensor_ != nullptr)
    ESP_LOGCONFIG(TAG, "    Station altitude: %.1f m", this->station_altitude_);
  LOG_SENSOR("  ", "Pressure Trend 3h", this->pressure_trend_3h_sensor_);
  LOG_SENSOR("  ", "Pressure Trend 24h", this->pressure_trend_24h_sensor_);
  LOG_TEXT_SENSOR("  ", "Pressure Tendency", this->pressure_tendency_text_sensor_);
  ESP_LOGCONFIG(TAG, "  Failure timeout: %u s", (unsigned) (this->failure_timeout_ms_ / 1000));
  ESP_LOGCONFIG(TAG, "  Calibration: T offset %.2f°C, RH = %.4f * raw %+.2f, p offset %+.2f hPa",
                this->temperature_offset_, this->humidity_gain_, this->humidity_offset_, this->pressure_offset_);
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

double GY39Component::enhancement_factor(double temperature_c, double pressure_pa) {
  const double p_hpa = pressure_pa / 100.0;
  return 1.0 + 1e-4 * (7.2 + p_hpa * (0.0320 + 5.9e-6 * temperature_c * temperature_c));
}

double GY39Component::vapor_pressure(double temperature_c, double rh_percent, double pressure_pa) {
  // Validity: BME280 operating range and range of the equations used.
  if (!std::isfinite(temperature_c) || !std::isfinite(rh_percent) || !std::isfinite(pressure_pa))
    return NAN;
  if (temperature_c < -45.0 || temperature_c > 85.0 || pressure_pa < 30000.0 || pressure_pa > 110000.0)
    return NAN;
  if (rh_percent < 0.0)
    rh_percent = 0.0;
  if (rh_percent > 100.0)
    rh_percent = 100.0;
  return rh_percent / 100.0 * enhancement_factor(temperature_c, pressure_pa) *
         saturation_vapor_pressure_water(temperature_c + 273.15);
}

double GY39Component::compute_absolute_humidity(double temperature_c, double rh_percent, double pressure_pa) {
  const double e = vapor_pressure(temperature_c, rh_percent, pressure_pa);
  if (std::isnan(e))
    return NAN;

  static const double R = 8.314462618;   // J/(mol·K), CODATA 2018
  static const double M_W = 18.01528e-3;  // kg/mol, molar mass of water

  const double t = temperature_c;
  const double T = t + 273.15;
  const double xv = e / pressure_pa;  // mole fraction of water vapour

  const double p_t = pressure_pa / T;
  const double z = 1.0 -
                   p_t * (1.58123e-6 - 2.9331e-8 * t + 1.1043e-10 * t * t + (5.707e-6 - 2.051e-8 * t) * xv +
                          (1.9898e-4 - 2.376e-6 * t) * xv * xv) +
                   p_t * p_t * (1.83e-11 - 0.765e-8 * xv * xv);

  return e * M_W / (z * R * T) * 1000.0;  // kg/m³ -> g/m³
}

// Dew point (WMO): the temperature at which the air, cooled at constant pressure and
// constant vapour content, becomes saturated with respect to liquid water:
//     f(p, Td) * e_w(Td) = e
// Solved numerically: Newton iteration on ln(), safeguarded by bisection in
// [-100 °C, t]. Converges to < 1e-6 K, normally in 3-4 iterations.
double GY39Component::compute_dew_point(double temperature_c, double rh_percent, double pressure_pa) {
  const double e = vapor_pressure(temperature_c, rh_percent, pressure_pa);
  if (std::isnan(e) || e <= 0.0)
    return NAN;  // RH = 0 % has no dew point

  const double ln_e = std::log(e);
  auto g = [&](double td_c) {
    return std::log(enhancement_factor(td_c, pressure_pa) * saturation_vapor_pressure_water(td_c + 273.15)) - ln_e;
  };

  double lo = -100.0;  // lower validity limit of Murphy & Koop is -150 °C
  double hi = temperature_c;
  if (g(lo) > 0.0)
    return NAN;  // dew point below -100 °C
  if (g(hi) <= 0.0)
    return temperature_c;  // saturated (RH = 100 %)

  // Initial guess: Magnus formula (Alduchov & Eskridge 1996).
  const double gamma = std::log(rh_percent / 100.0) + 17.625 * temperature_c / (243.04 + temperature_c);
  double td = 243.04 * gamma / (17.625 - gamma);
  if (!(td > lo && td < hi))
    td = 0.5 * (lo + hi);

  for (int i = 0; i < 50; i++) {
    const double gv = g(td);
    if (gv > 0.0) {
      hi = td;
    } else {
      lo = td;
    }
    const double h = 1e-4;
    const double dg = (g(td + h) - g(td - h)) / (2.0 * h);
    double next = td - gv / dg;
    if (!(next > lo && next < hi))
      next = 0.5 * (lo + hi);  // Newton step left the bracket -> bisect
    if (std::fabs(next - td) < 1e-6)
      return next;
    td = next;
  }
  return td;
}

void GY39Component::apply_calibration(double t_raw, double rh_raw, double p_pa, double t_offset, double rh_gain,
                                      double rh_offset, double *t_out, double *rh_out) {
  double rh_cal = rh_gain * rh_raw + rh_offset;
  if (rh_cal < 0.0)
    rh_cal = 0.0;
  if (rh_cal > 100.0)
    rh_cal = 100.0;
  const double t = t_raw + t_offset;
  double rh = rh_cal;
  if (t_offset != 0.0) {
    const double e = vapor_pressure(t_raw, rh_cal, p_pa);
    if (std::isfinite(e) && t > -100.0) {
      rh = e / (enhancement_factor(t, p_pa) * saturation_vapor_pressure_water(t + 273.15)) * 100.0;
      if (rh > 100.0)
        rh = 100.0;
    }
  }
  *t_out = t;
  *rh_out = rh;
}

// Mixing ratio w = eps * e / (p - e), eps = M_w / M_a = 0.621945 (ASHRAE Fundamentals).
double GY39Component::compute_mixing_ratio(double temperature_c, double rh_percent, double pressure_pa) {
  const double e = vapor_pressure(temperature_c, rh_percent, pressure_pa);
  if (std::isnan(e) || e >= pressure_pa)
    return NAN;
  return 0.621945 * e / (pressure_pa - e) * 1000.0;  // kg/kg -> g/kg
}

// Reduction to mean sea level, Deutscher Wetterdienst formula:
//   p0 = p * exp( g * h / (R_d * T_h) ),   T_h = T + C_h * E + a * h / 2
// g = 9.80665 m/s², R_d = 287.05 J/(kg·K), a = 0.0065 K/m (mean lapse rate),
// C_h = 0.12 K/hPa (humidity correction), E = vapour pressure in hPa.
double GY39Component::compute_sea_level_pressure(double temperature_c, double rh_percent, double pressure_pa,
                                                 double station_altitude_m) {
  if (std::isnan(station_altitude_m))
    return NAN;
  const double e = vapor_pressure(temperature_c, rh_percent, pressure_pa);
  if (std::isnan(e))
    return NAN;
  const double t_h = temperature_c + 273.15 + 0.12 * (e / 100.0) + 0.0065 * station_altitude_m / 2.0;
  return pressure_pa * std::exp(9.80665 * station_altitude_m / (287.05 * t_h));
}

// UK Met Office / WMO barometric tendency descriptors (change over 3 hours):
//   < 0.1 hPa steady, 0.1-1.5 slowly, 1.6-3.5 (normal), 3.6-6.0 quickly, > 6.0 very rapidly.
int GY39Component::tendency_class(double change_3h_hpa) {
  const double a = std::round(std::fabs(change_3h_hpa) * 10.0) / 10.0;  // descriptors use 0.1 hPa steps
  int level;
  if (a < 0.1) {
    level = 0;
  } else if (a <= 1.5) {
    level = 1;
  } else if (a <= 3.5) {
    level = 2;
  } else if (a <= 6.0) {
    level = 3;
  } else {
    level = 4;
  }
  return change_3h_hpa < 0 ? -level : level;
}

float GY39Component::history_ago_(uint16_t bins) const {
  if (bins >= this->history_count_)
    return NAN;
  uint16_t idx = (this->history_head_ + HISTORY_BINS - 1 - bins) % HISTORY_BINS;
  return this->history_[idx];
}

void GY39Component::update_pressure_history_(double pressure_pa) {
  const uint32_t now = millis();
  if (!this->bin_started_) {
    this->bin_started_ = true;
    this->bin_start_ = now;
  }
  // Close every 10-minute bin that has ended (empty bins are stored as NAN).
  bool closed = false;
  uint16_t guard = 0;
  while (now - this->bin_start_ >= BIN_MS && guard++ < HISTORY_BINS) {
    float mean = this->bin_n_ > 0 ? static_cast<float>(this->bin_sum_ / this->bin_n_) : NAN;
    this->history_[this->history_head_] = mean;
    this->history_head_ = (this->history_head_ + 1) % HISTORY_BINS;
    if (this->history_count_ < HISTORY_BINS)
      this->history_count_++;
    this->bin_sum_ = 0.0;
    this->bin_n_ = 0;
    this->bin_start_ += BIN_MS;
    closed = true;
  }
  if (now - this->bin_start_ >= BIN_MS)
    this->bin_start_ = now;  // device was stalled for > 24 h: restart binning
  if (std::isfinite(pressure_pa)) {
    this->bin_sum_ += pressure_pa;
    this->bin_n_++;
  }
  if (closed)
    this->publish_trends_();
}

void GY39Component::publish_trends_() {
  const float now_pa = this->history_ago_(0);
  const float d3 = (now_pa - this->history_ago_(18)) / 100.0f;    // 18 bins = 3 h
  const float d24 = (now_pa - this->history_ago_(144)) / 100.0f;  // 144 bins = 24 h
  ESP_LOGD(TAG, "Pressure trend: 3h %.2f hPa, 24h %.2f hPa (%u bins)", d3, d24, this->history_count_);
  if (this->pressure_trend_3h_sensor_ != nullptr)
    this->pressure_trend_3h_sensor_->publish_state(d3);
  if (this->pressure_trend_24h_sensor_ != nullptr)
    this->pressure_trend_24h_sensor_->publish_state(d24);
  if (this->pressure_tendency_text_sensor_ != nullptr) {
    static const char *const EN[] = {"Falling very rapidly", "Falling quickly", "Falling", "Falling slowly",
                                     "Steady", "Rising slowly", "Rising", "Rising quickly", "Rising very rapidly"};
    static const char *const ZH[] = {"非常快速下降", "快速下降", "下降", "緩慢下降", "平穩",
                                     "緩慢上升",     "上升",     "快速上升", "非常快速上升"};
    std::string text;
    if (std::isnan(d3) && this->history_count_ <= 18) {
      text = this->tendency_zh_ ? "資料收集中" : "Collecting data";
    } else if (std::isnan(d3)) {
      text = this->tendency_zh_ ? "無資料" : "No data";  // a gap (read failures) at either end of the 3 h window
    } else {
      int idx = tendency_class(d3) + 4;
      text = this->tendency_zh_ ? ZH[idx] : EN[idx];
    }
    if (!this->pressure_tendency_text_sensor_->has_state() || this->pressure_tendency_text_sensor_->state != text)
      this->pressure_tendency_text_sensor_->publish_state(text);
  }
}

void GY39Component::report_failure_(bool lux, bool bme) {
  const uint32_t now = millis();
  if (bme)
    this->update_pressure_history_(NAN);  // keeps 10-minute bins advancing during an outage
  if (lux && now - this->last_lux_ok_ >= this->failure_timeout_ms_ && this->illuminance_sensor_ != nullptr &&
      !std::isnan(this->illuminance_sensor_->state)) {
    ESP_LOGW(TAG, "No illuminance reading for %u s, publishing NaN", (unsigned) ((now - this->last_lux_ok_) / 1000));
    this->illuminance_sensor_->publish_state(NAN);
  }
  if (bme && now - this->last_bme_ok_ >= this->failure_timeout_ms_) {
    sensor::Sensor *const live[] = {this->temperature_sensor_,       this->pressure_sensor_,
                                    this->humidity_sensor_,          this->altitude_sensor_,
                                    this->absolute_humidity_sensor_, this->dew_point_sensor_,
                                    this->mixing_ratio_sensor_,      this->sea_level_pressure_sensor_};
    bool any = false;
    for (auto *s : live) {
      if (s != nullptr && !std::isnan(s->state)) {
        s->publish_state(NAN);
        any = true;
      }
    }
    if (any)
      ESP_LOGW(TAG, "No BME280 reading for %u s, publishing NaN", (unsigned) ((now - this->last_bme_ok_) / 1000));
  }
}

void GY39Component::publish_lux_(uint32_t raw) {
  this->last_lux_ok_ = millis();
  float lux = raw / 100.0f;
  ESP_LOGD(TAG, "Illuminance: %.2f lx", lux);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(lux);
}

void GY39Component::publish_bme_(int16_t temp, uint32_t pressure, uint16_t humidity, int16_t altitude) {
  this->last_bme_ok_ = millis();
  const double p_pa = pressure / 100.0 + this->pressure_offset_ * 100.0;  // raw is Pa*100
  double t_c, rh;
  apply_calibration(temp / 100.0, humidity / 100.0, p_pa, this->temperature_offset_, this->humidity_gain_,
                    this->humidity_offset_, &t_c, &rh);
  ESP_LOGD(TAG, "Raw: %.2f°C %.2f%% %.2f hPa -> calibrated: %.2f°C %.2f%% %.2f hPa, altitude %d m", temp / 100.0,
           humidity / 100.0, pressure / 10000.0, t_c, rh, p_pa / 100.0, altitude);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(t_c);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(p_pa / 100.0);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(rh);
  if (this->altitude_sensor_ != nullptr)
    this->altitude_sensor_->publish_state(altitude);

  if (this->absolute_humidity_sensor_ != nullptr) {
    double ah = compute_absolute_humidity(t_c, rh, p_pa);
    if (std::isnan(ah)) {
      ESP_LOGW(TAG, "Inputs out of range, absolute humidity not calculated");
      this->absolute_humidity_sensor_->publish_state(NAN);
    } else {
      ESP_LOGD(TAG, "Absolute humidity: %.3f g/m³", ah);
      this->absolute_humidity_sensor_->publish_state(static_cast<float>(ah));
    }
  }

  if (this->dew_point_sensor_ != nullptr) {
    double td = compute_dew_point(t_c, rh, p_pa);
    if (std::isnan(td)) {
      ESP_LOGW(TAG, "Inputs out of range, dew point not calculated");
    } else {
      ESP_LOGD(TAG, "Dew point: %.3f°C", td);
    }
    this->dew_point_sensor_->publish_state(static_cast<float>(td));
  }

  if (this->mixing_ratio_sensor_ != nullptr) {
    double w = compute_mixing_ratio(t_c, rh, p_pa);
    ESP_LOGD(TAG, "Mixing ratio: %.3f g/kg", w);
    this->mixing_ratio_sensor_->publish_state(static_cast<float>(w));
  }

  if (this->sea_level_pressure_sensor_ != nullptr) {
    double p0 = compute_sea_level_pressure(t_c, rh, p_pa, this->station_altitude_);
    ESP_LOGD(TAG, "Sea level pressure: %.2f hPa", p0 / 100.0);
    this->sea_level_pressure_sensor_->publish_state(static_cast<float>(p0 / 100.0));
  }

  if (this->pressure_trend_3h_sensor_ != nullptr || this->pressure_trend_24h_sensor_ != nullptr ||
      this->pressure_tendency_text_sensor_ != nullptr) {
    this->update_pressure_history_((p_pa >= 30000.0 && p_pa <= 110000.0) ? p_pa : NAN);
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
                    this->absolute_humidity_sensor_ != nullptr || this->dew_point_sensor_ != nullptr ||
                    this->mixing_ratio_sensor_ != nullptr || this->sea_level_pressure_sensor_ != nullptr ||
                    this->pressure_trend_3h_sensor_ != nullptr || this->pressure_trend_24h_sensor_ != nullptr ||
                    this->pressure_tendency_text_sensor_ != nullptr;
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
    this->report_failure_(this->want_lux_, this->want_bme_);
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
    this->report_failure_(true, true);
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
