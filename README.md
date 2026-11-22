# ESPHome GY-39 External Component

**English** | [繁體中文](README.zh-TW.md)

An [ESPHome](https://esphome.io) external component for the **GY-39** environmental sensor module
(MAX44009 ambient light + BME280 temperature / humidity / pressure, with an on-board MCU).
Both **UART** and **I2C** interfaces are supported.

| Sensor key    | Unit | Description                                       |
|---------------|------|---------------------------------------------------|
| `illuminance` | lx   | Ambient light (0.045 – 188,000 lx)                |
| `temperature` | °C   | Temperature                                       |
| `pressure`    | hPa  | Barometric pressure                               |
| `humidity`    | %    | Relative humidity                                 |
| `altitude`    | m    | Altitude estimated by the module from pressure    |
| `absolute_humidity` | g/m³ | Water vapour density, **calculated** from temperature, RH and pressure ([details](#absolute-humidity-and-dew-point)) |
| `dew_point`   | °C   | Dew point, **calculated** from the same data ([details](#absolute-humidity-and-dew-point)) |

Every sensor is optional and accepts all standard
[Sensor](https://esphome.io/components/sensor/) options (`name`, `id`, `filters`, …).

---

## Table of contents

- [Installation](#installation)
- [Choosing UART or I2C](#choosing-uart-or-i2c)
- [Wiring](#wiring)
- [UART example](#uart-example)
- [I2C example](#i2c-example)
- [Configuration variables](#configuration-variables)
- [Recommendations](#recommendations)
- [Troubleshooting](#troubleshooting)
- [Absolute humidity and dew point](#absolute-humidity-and-dew-point)
- [Protocol notes](#protocol-notes)

---

## Installation

```yaml
external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]
```

To pin a specific version, replace `@main` with a tag or commit hash.

---

## Choosing UART or I2C

The **S0 pin** selects the module's interface, and the module reads it **at power-up**:

| S0 at power-up       | Mode | `interface:` in YAML |
|----------------------|------|----------------------|
| Floating (unconnected) | UART (factory default) | `uart` (default) |
| Tied to **GND**      | I2C  | `i2c`                |

- The `interface:` option only tells ESPHome how to talk to the module. It **must match** the S0 wiring.
- After changing S0, **power-cycle** the module. A reset of the ESP alone is not enough if the module stays powered.
- One module can use only one mode at a time, because CT/DR are shared pins.
  Multiple modules can mix modes in the same configuration.

| | UART | I2C |
|---|---|---|
| Wiring | 2 dedicated pins (TX/RX) | Shares the I2C bus with other sensors |
| Data integrity | Checksum verified on every frame | No checksum |
| Setup | Works out of the box | Needs S0 → GND and a low bus speed |
| Multiple modules | One UART per module | Only one per bus (fixed address 0x5B) |
| **Recommended for** | **Most users** | Short of free pins, or sharing an existing I2C bus |

---

## Wiring

| GY-39 pin | UART mode                         | I2C mode                    |
|-----------|-----------------------------------|-----------------------------|
| VCC       | 3.3 V or 5 V                      | 3.3 V or 5 V                |
| GND       | GND                               | GND                         |
| CT        | Module **TX** → ESP **RX** pin    | **SCL**                     |
| DR        | Module **RX** → ESP **TX** pin    | **SDA**                     |
| S0        | Leave floating                    | **GND (before power-up)**   |

> TX/RX are **crossed**: the module's TX (CT) goes to the ESP's RX, and the module's RX (DR) goes to the ESP's TX.

---

## UART example

### ESP32

ESP32 has spare hardware UARTs, so this is the easiest setup.

```yaml
esphome:
  name: gy39-uart

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]

uart:
  id: gy39_uart
  tx_pin: GPIO17   # -> GY-39 DR (module RX)
  rx_pin: GPIO16   # <- GY-39 CT (module TX)
  baud_rate: 9600  # factory default

sensor:
  - platform: gy39
    interface: uart        # default, may be omitted
    uart_id: gy39_uart
    update_interval: 10s
    illuminance:
      name: "GY39 Illuminance"
    temperature:
      name: "GY39 Temperature"
    pressure:
      name: "GY39 Pressure"
    humidity:
      name: "GY39 Humidity"
    altitude:
      name: "GY39 Altitude"
    absolute_humidity:
      name: "GY39 Absolute Humidity"
    dew_point:
      name: "GY39 Dew Point"
```

### ESP8266

ESP8266 has only one full hardware UART (GPIO1/GPIO3), which the logger uses by default.
Either disable logger output on UART, or use the software-serial pins:

```yaml
logger:
  baud_rate: 0          # free hardware UART0 for the sensor

uart:
  id: gy39_uart
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600
```

or, keeping serial logging:

```yaml
uart:
  id: gy39_uart
  tx_pin: GPIO14        # D5, software serial
  rx_pin: GPIO12        # D6, software serial
  baud_rate: 9600       # 9600 is fine for software serial
```

**How UART mode works:** on every `update_interval` the component sends the
"query once" command `A5 53 F8` and publishes the two response frames (light and BME280).
If the module is in continuous-output mode, the extra frames are ignored,
so values are still published only once per `update_interval`.

---

## I2C example

1. Tie **S0 to GND**, then power up the module.
2. Use a low I2C bus frequency.

```yaml
esphome:
  name: gy39-i2c

esp8266:
  board: d1_mini

logger:

external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]

i2c:
  sda: GPIO4        # D2 -> GY-39 DR
  scl: GPIO5        # D1 -> GY-39 CT
  frequency: 50kHz  # recommended, the module's MCU is slow
  scan: true        # should report a device at 0x5B

sensor:
  - platform: gy39
    interface: i2c
    address: 0x5B   # default, may be omitted
    update_interval: 10s
    illuminance:
      name: "GY39 Illuminance"
    temperature:
      name: "GY39 Temperature"
    pressure:
      name: "GY39 Pressure"
    humidity:
      name: "GY39 Humidity"
    altitude:
      name: "GY39 Altitude"
    absolute_humidity:
      name: "GY39 Absolute Humidity"
    dew_point:
      name: "GY39 Dew Point"
```

For ESP32, use any two GPIOs, for example `sda: GPIO21` and `scl: GPIO22`, with the same `frequency: 50kHz`.

**How I2C mode works:** on every `update_interval` the component reads registers `0x00`–`0x0D`
(14 bytes) in a single transaction and publishes all enabled sensors.

---

## Configuration variables

| Option            | Mode | Required | Default | Description |
|-------------------|------|----------|---------|-------------|
| `interface`       | all  | no       | `uart`  | `uart` or `i2c` |
| `uart_id`         | UART | no\*     | –       | ID of the `uart:` bus (\*needed when more than one UART is defined) |
| `i2c_id`          | I2C  | no\*     | –       | ID of the `i2c:` bus (\*needed when more than one bus is defined) |
| `address`         | I2C  | no       | `0x5B`  | I2C address |
| `update_interval` | all  | no       | `60s`   | How often to read and publish |
| `illuminance`     | all  | no       | –       | Light sensor (lx) |
| `temperature`     | all  | no       | –       | Temperature sensor (°C) |
| `pressure`        | all  | no       | –       | Pressure sensor (hPa) |
| `humidity`        | all  | no       | –       | Humidity sensor (%) |
| `altitude`        | all  | no       | –       | Altitude sensor (m) |
| `absolute_humidity` | all | no      | –       | Absolute humidity (g/m³), calculated |
| `dew_point`       | all  | no       | –       | Dew point (°C), calculated |

---

## Recommendations

- **Prefer UART** unless you have a specific reason to use I2C. It is the factory default,
  needs no extra wiring, and every frame is checksum-verified.
- **`update_interval`:** 10 s – 60 s is plenty for environmental data.
  Avoid going below about 2 s, because the module itself refreshes only a few times per second.
- **I2C bus speed:** keep it at **50 kHz** (≤ 100 kHz). The module's MCU answers slowly,
  and high speeds cause read errors.
- **I2C pull-ups:** most GY-39 boards include pull-ups. If you have long wires or several
  devices on the bus, add 4.7 kΩ pull-ups to 3.3 V.
- **Power:** the board has its own regulator, so 3.3 V or 5 V both work.
  The logic level follows VCC, so **power it from 3.3 V when connected to an ESP** to keep the signals at 3.3 V.
- **Temperature accuracy:** the BME280 is self-heated by nearby electronics.
  Mount the module away from the ESP and voltage regulators, or correct it with a filter:

  ```yaml
  temperature:
    name: "GY39 Temperature"
    filters:
      - offset: -1.5
  ```

- **Smoothing noisy light readings:**

  ```yaml
  illuminance:
    name: "GY39 Illuminance"
    filters:
      - sliding_window_moving_average:
          window_size: 5
          send_every: 1
  ```

- **Altitude** is calculated by the module from a standard sea-level pressure,
  so it is only a rough estimate. Disable it if you don't need it.

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| UART: `Timeout waiting for GY-39 response` | TX/RX not crossed, wrong `baud_rate`, the logger occupying UART0 (ESP8266), or S0 tied to GND (I2C mode) |
| UART: `Checksum mismatch` | Electrical noise or a baud mismatch. Shorten the wires and check the ground |
| UART: no data after changing the baud rate | If you sent the 115200 command (`A5 AF 54`) earlier, set `baud_rate: 115200` |
| I2C: `Communication with GY-39 failed` | S0 not tied to GND **before power-up**. Power-cycle the module. Check that `scan: true` finds 0x5B |
| I2C: intermittent read errors | Lower `frequency` (for example 50 kHz or 20 kHz), shorten the wires, add pull-ups |
| Temperature reads too high | Self-heating, see [Recommendations](#recommendations) |

Enable verbose logs to see raw values:

```yaml
logger:
  level: DEBUG
  logs:
    gy39: DEBUG
```

---

## Absolute humidity and dew point

Both values are calculated on the ESP from the module's own temperature, RH and pressure,
using the same reference-grade equations.

### Absolute humidity

`absolute_humidity` is the mass of water vapour per cubic metre of air (g/m³).
The module does not measure it directly. The component calculates it on the ESP, in double precision,
from the temperature, relative humidity and pressure that the module reports in the same reading.
It does not use the textbook Magnus approximation. Instead it uses reference-grade equations:

**1. Actual vapour pressure** (WMO definition of relative humidity, referenced to liquid water):

$$e = \frac{RH}{100}\cdot f(p,t)\cdot e_w(T)$$

**2. Vapour density** (real-gas form):

$$\rho_v = \frac{e \cdot M_w}{Z \cdot R \cdot T}$$

| Term | Equation used | Notes |
|---|---|---|
| $e_w(T)$, T ≥ 0.01 °C | **Wagner & Pruss (2002)**, IAPWS saturation-pressure equation | Deviates from IAPWS-95 by < 0.01 % |
| $e_w(T)$, T < 0.01 °C | **Murphy & Koop (2005)**, supercooled liquid water | Both equations give 611.657 Pa at the triple point, so the curve is continuous |
| $f(p,t)$ | **Buck (1996)** enhancement factor $1+10^{-4}[7.2+P(0.0320+5.9\cdot10^{-6}t^2)]$ | Corrects for water vapour in air vs. pure vapour (≈ +0.4 % at 1013 hPa) |
| $Z$ | **CIPM-2007** moist-air compressibility (Picard et al., *Metrologia* 45, 2008) | ≈ 0.9996 near room conditions |
| $R$, $M_w$ | 8.314462618 J/(mol·K) (CODATA 2018), 18.01528 g/mol | |

**Input handling**
- RH is clamped to 0–100 %.
- Results are only published for −45…85 °C and 300…1100 hPa (the BME280 range).
  Outside that range the sensor publishes `NaN` and logs a warning.
- The measured pressure is used, so the result stays correct at altitude.

**Accuracy.** The equations add < 0.1 % error. The real limit is the BME280 itself
(typically ±3 % RH and ±1 °C). At 25 °C, absolute humidity changes about 5.6 % per °C and
proportionally with RH. So at 50 % RH, expect about **±8 %** overall (root-sum-square of both errors).
To improve it, keep the module away from heat sources, which matters most, or calibrate the
temperature with an `offset` filter.

| Check (1013.25 hPa) | Result |
|---|---|
| 20 °C, 50 % RH | 8.68 g/m³ |
| 30 °C, 100 % RH | 30.50 g/m³ |
| 0 °C, 100 % RH | 4.87 g/m³ |
| −10 °C, 80 % RH | 1.90 g/m³ |

### Dew point

`dew_point` follows the WMO definition: the temperature to which the air must be cooled,
at constant pressure and constant water-vapour content, to become saturated **with respect to liquid water**.
It is the exact solution of

$$f(p,T_d)\cdot e_w(T_d) = e$$

using the same $e_w$ (Wagner & Pruss / Murphy & Koop) and $f$ (Buck) as above.
The equation is solved numerically: a Newton iteration starts from a Magnus estimate and is
safeguarded by bisection in [−100 °C, t]. It converges to < 10⁻⁶ K, normally in 3–4 iterations.
It does **not** use the Magnus approximation for the final result.
Compared with an independent bisection solver over −40…85 °C, 5…100 % RH and 700…1100 hPa (1560 points),
the largest difference is 9.3 × 10⁻⁷ K.

- RH = 100 % (or above) → dew point = air temperature.
- RH = 0 %, or dew point below −100 °C → `NaN`.
- Below 0 °C the result is the dew point over **supercooled water**, as defined by WMO.
  This is not the frost point, which is higher.

**Accuracy.** As with absolute humidity, the limit is the sensor:

| Conditions | ±3 % RH | ±1 °C | Combined (RSS) |
|---|---|---|---|
| 20 °C, 50 % RH (Td = 9.28 °C) | ±0.89 K | ±0.92 K | **±1.3 K** |
| 25 °C, 60 % RH (Td = 16.71 °C) | ±0.79 K | ±0.94 K | **±1.2 K** |
| 0 °C, 80 % RH (Td = −3.03 °C) | ±0.50 K | ±0.98 K | **±1.1 K** |

| Check (1013.25 hPa) | Dew point |
|---|---|
| 20 °C, 50 % RH | 9.28 °C |
| 25 °C, 60 % RH | 16.71 °C |
| 30 °C, 80 % RH | 26.17 °C |
| 35 °C, 30 % RH | 14.85 °C |
| 0 °C, 50 % RH | −9.18 °C |
| −10 °C, 80 % RH | −12.79 °C |

---

## Protocol notes

**UART frames** (module → host, checksum = low byte of the sum of all previous bytes):

| Frame | Bytes |
|---|---|
| Light   | `5A 5A 15 04` LUX(4, ÷100) `SUM` |
| BME280  | `5A 5A 45 0A` TEMP(2, signed, ÷100) PRESS(4, ÷100 Pa) HUM(2, ÷100) ALT(2, m) `SUM` |

**Commands** (host → module): `A5 53 F8` = query light + BME280 once (used by this component).

**I2C registers** (address 0x5B, big-endian): `0x00–0x03` lux×100, `0x04–0x05` temp×100,
`0x06–0x09` pressure Pa×100, `0x0A–0x0B` humidity×100, `0x0C–0x0D` altitude m.

---

## License

[MIT](LICENSE)
