#include "gy39.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

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
                    this->humidity_sensor_ != nullptr || this->altitude_sensor_ != nullptr;
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
