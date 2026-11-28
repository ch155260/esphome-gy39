# ESPHome GY-39 External Component

**English** | [繁體中文](README.zh-TW.md)

An [ESPHome](https://esphome.io) external component for the **GY-39** environmental sensor module
(MAX44009 ambient light + BME280 temperature / humidity / pressure, with an on-board MCU).
Both **UART** and **I2C** interfaces are supported.

**Measured by the module**

| Sensor key    | Unit | Description                                       |
|---------------|------|---------------------------------------------------|
| `illuminance` | lx   | Ambient light (0.045 – 188,000 lx)                |
| `temperature` | °C   | Temperature                                       |
| `humidity`    | %    | Relative humidity                                 |
| `pressure`    | hPa  | Barometric (station) pressure                     |
| `altitude`    | m    | Altitude estimated by the module, integer metres. *Not recommended*, see [Which values to trust](#which-values-to-trust) |

**Calculated by this component** (see [Calculated values](#calculated-values))

| Sensor key           | Unit | Description |
|----------------------|------|-------------|
| `dew_point`          | °C   | Dew point (WMO, over liquid water) |
| `absolute_humidity`  | g/m³ | Water vapour density |
| `mixing_ratio`       | g/kg | Mass of water vapour per kilogram of dry air |
| `sea_level_pressure` | hPa  | Pressure reduced to mean sea level (needs `station_altitude`) |
| `pressure_trend_3h`  | hPa  | Pressure change over the last 3 hours (WMO standard) |
| `pressure_trend_24h` | hPa  | Pressure change over the last 24 hours (removes the daily pressure cycle) |
| `pressure_tendency`  | text | Tendency description, e.g. "Falling slowly" (text sensor) |

Every sensor is optional and accepts all standard
[Sensor](https://esphome.io/components/sensor/) options (`name`, `id`, `filters`, …).
Calculated values are only computed when they are configured.

---

## Table of contents

- [Installation](#installation)
- [Package (quickest setup)](#package-quickest-setup)
- [Manual configuration](#manual-configuration)
- [Choosing UART or I2C](#choosing-uart-or-i2c)
- [Wiring](#wiring)
- [UART example](#uart-example)
- [I2C example](#i2c-example)
- [Configuration variables](#configuration-variables)
- [Recommendations](#recommendations)
- [Mounting location](#mounting-location)
- [Calibration](#calibration)
- [Troubleshooting](#troubleshooting)
- [Calculated values](#calculated-values)
  - [Which values to trust](#which-values-to-trust)
- [Protocol notes](#protocol-notes)

---

## Installation

**Recommended: import the package.** One `packages:` block adds the component, the bus (UART)
and all recommended sensors. See [Package](#package-quickest-setup), plus the complete
[UART](#uart-example) and [I2C](#i2c-example) examples.

**Alternative: load only the component** and write the sensor configuration yourself
(see [Manual configuration](#manual-configuration)):

```yaml
external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]
```

To pin a specific version, replace `@main` (and the package `ref`) with a tag or commit hash.

---

## Package (quickest setup)

Instead of writing `external_components` and every sensor yourself, you can import a
ready-made [package](https://esphome.io/components/packages/) from this repository:

```yaml
packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main                 # or a tag / commit to pin a version
    refresh: 1d
    files:
      - path: packages/gy39-uart.yaml      # or packages/gy39-i2c.yaml
        vars:
          gy39_station_altitude: "31"      # required
          gy39_tx_pin: GPIO17
          gy39_rx_pin: GPIO16
```

| Package | Bus |
|---|---|
| `packages/gy39-uart.yaml` | Creates its own UART bus `${gy39_id}_uart` |
| `packages/gy39-i2c.yaml` | Uses the `i2c:` bus **from your main config** (define it there, `frequency: 50kHz` recommended) |

Both add the external component and the recommended sensors (the full list is in [Manual configuration](#manual-configuration)).
Complete examples: [`examples/gy39-uart.yaml`](examples/gy39-uart.yaml), [`examples/gy39-i2c.yaml`](examples/gy39-i2c.yaml).

**Variables.** Set them under `vars:`. The package's own `defaults:` take precedence over top-level `substitutions:`,
so a top-level substitution only works for `gy39_station_altitude`, which has no default.

| Variable | Package | Default | Description |
|---|---|---|---|
| `gy39_station_altitude` | both | **required** | Sensor height above sea level (m) |
| `gy39_id` | both | `gy39` | Prefix of every ID (`gy39_sensor`, `gy39_temperature`, …). Use different values for several modules |
| `gy39_name_prefix` | both | `""` | Entity name prefix, e.g. `"Balcony "` or `"陽台 "` (note the trailing space). With several modules the prefixes must differ in ASCII letters or digits, because non-ASCII characters become `_` in ESPHome's internal IDs |
| `gy39_update_interval` | both | `60s` | Polling interval |
| `gy39_tendency_language` | both | `en` | `en` or `zh-TW` |
| `gy39_temperature_offset` | both | `0` | °C, see [Calibration](#calibration) |
| `gy39_pressure_offset` | both | `0` | hPa |
| `gy39_failure_timeout` | both | `60s` | See `failure_timeout` in [Configuration variables](#configuration-variables) |
| `gy39_ref` | both | `main` | Git ref of the component. Set it to the same value as the package `ref` |
| `gy39_tx_pin` / `gy39_rx_pin` | UART | `GPIO17` / `GPIO16` | ESP TX → GY-39 DR, ESP RX ← GY-39 CT |
| `gy39_baud_rate` | UART | `9600` | UART baud rate |
| `gy39_address` | I2C | `0x5B` | I2C address |

**Changing the package.** The platform entry has the ID `${gy39_id}_sensor` (not just `gy39`, because that name
is reserved for the integration). Use `!extend` to change options and `!remove` to drop sensors:

```yaml
sensor:
  - id: !extend gy39_sensor
    humidity_calibration: ["34.0 -> 32.8", "77.5 -> 75.3"]
    illuminance: !remove
    mixing_ratio: !remove
```

**Several I2C buses or several modules.** Choose the bus with `i2c_id`. Each module needs its own `gy39_id`:

```yaml
i2c:
  - {id: bus_a, sda: GPIO21, scl: GPIO22, frequency: 50kHz}
  - {id: bus_b, sda: GPIO18, scl: GPIO19, frequency: 50kHz}
packages:
  indoor:
    url: https://github.com/ch155260/esphome-gy39
    files:
      - path: packages/gy39-i2c.yaml
        vars: {gy39_id: indoor, gy39_name_prefix: "Indoor ", gy39_station_altitude: "31"}
  outdoor:
    url: https://github.com/ch155260/esphome-gy39
    files:
      - path: packages/gy39-i2c.yaml
        vars: {gy39_id: outdoor, gy39_name_prefix: "Outdoor ", gy39_station_altitude: "31"}
sensor:
  - id: !extend indoor_sensor
    i2c_id: bus_a
  - id: !extend outdoor_sensor
    i2c_id: bus_b
```

On ESP8266 with the UART package on GPIO1/GPIO3, also set `logger: baud_rate: 0`.

---

## Manual configuration

Without the package, load the component with `external_components` (see [Installation](#installation))
and write the configuration yourself. The block below is what the package sets up: only values with a high
reference value (see [Which values to trust](#which-values-to-trust)). You also need a `uart:` or `i2c:` bus.

```yaml
sensor:
  - platform: gy39
    interface: uart            # or i2c
    uart_id: gy39_uart
    update_interval: 60s
    station_altitude: 35       # m above sea level = ground elevation + mounting height
    tendency_language: en      # or zh-TW
    # Calibration, see "Calibration". Applied before every calculated value.
    # temperature_offset: -0.8
    # humidity_calibration:
    #   - 34.0 -> 32.8
    #   - 77.5 -> 75.3

    # Measured
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
    pressure:
      name: "Pressure"
    illuminance:
      name: "Illuminance"

    # Calculated (not affected by self-heating, high reference value)
    dew_point:
      name: "Dew Point"
    absolute_humidity:
      name: "Absolute Humidity"
    mixing_ratio:
      name: "Mixing Ratio"
    sea_level_pressure:
      name: "Sea Level Pressure"
    pressure_trend_3h:
      name: "Pressure Trend 3h"
    pressure_trend_24h:
      name: "Pressure Trend 24h"
    pressure_tendency:
      name: "Pressure Tendency"
```

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
Wiring: CT → GPIO16 (ESP RX), DR → GPIO17 (ESP TX), S0 unconnected.

```yaml
esphome:
  name: gy39-uart
  friendly_name: GY39

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-uart.yaml
        vars:
          gy39_station_altitude: "35"   # required: sensor height above sea level (m)
          gy39_tx_pin: GPIO17           # ESP TX -> GY-39 DR (module RX)
          gy39_rx_pin: GPIO16           # ESP RX <- GY-39 CT (module TX)
```

### ESP8266

ESP8266 has only one full hardware UART (GPIO1/GPIO3), which the logger uses by default.
Either free it from the logger:

```yaml
logger:
  baud_rate: 0                          # free hardware UART0 for the sensor

packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-uart.yaml
        vars:
          gy39_station_altitude: "35"
          gy39_tx_pin: GPIO1
          gy39_rx_pin: GPIO3
```

or keep serial logging and use software serial on other pins:

```yaml
packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-uart.yaml
        vars:
          gy39_station_altitude: "35"
          gy39_tx_pin: GPIO14           # D5, software serial
          gy39_rx_pin: GPIO12           # D6, software serial (9600 baud is fine)
```

**How UART mode works:** on every `update_interval` the component sends the
"query once" command `A5 53 F8` and publishes the two response frames (light and BME280).
If the module is in continuous-output mode, the extra frames are ignored,
so values are still published only once per `update_interval`.

---

## I2C example

1. Tie **S0 to GND**, then power up the module.
2. Define the I2C bus in your config (the I2C package uses it) with a low frequency.

```yaml
esphome:
  name: gy39-i2c
  friendly_name: GY39

esp8266:
  board: d1_mini

logger:

i2c:
  sda: GPIO4          # D2 -> GY-39 DR
  scl: GPIO5          # D1 -> GY-39 CT
  frequency: 50kHz    # recommended, the module's MCU is slow
  scan: true          # the log should show a device at 0x5B

packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-i2c.yaml
        vars:
          gy39_station_altitude: "35"   # required: sensor height above sea level (m)
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
| `altitude`        | all  | no       | –       | Altitude sensor (m), from the module |
| `dew_point`       | all  | no       | –       | Dew point (°C), calculated |
| `absolute_humidity` | all | no      | –       | Absolute humidity (g/m³), calculated |
| `mixing_ratio`    | all  | no       | –       | Mixing ratio (g/kg), calculated |
| `sea_level_pressure` | all | no     | –       | Sea-level pressure (hPa), calculated. Requires `station_altitude` |
| `station_altitude` | all | with `sea_level_pressure` | – | Height of the **sensor** above mean sea level in m (−500 … 9000) |
| `pressure_trend_3h` | all | no      | –       | Pressure change over 3 h (hPa) |
| `pressure_trend_24h` | all | no     | –       | Pressure change over 24 h (hPa) |
| `pressure_tendency` | all | no      | –       | Text sensor with the tendency description |
| `tendency_language` | all | no      | `en`    | `en` or `zh-TW` for `pressure_tendency` |
| `temperature_offset` | all | no     | `0`     | °C added to the temperature. RH is re-evaluated, see [Calibration](#calibration) |
| `humidity_calibration` | all | no   | –       | 1 or 2 points `measured -> true` (%RH) |
| `pressure_offset` | all  | no       | `0`     | hPa added to the pressure |
| `failure_timeout` | all  | no       | `60s`   | When no good reading arrives for this long, all live and calculated values are published as unknown (NaN) once, and the tendency text shows "No data". `0s` = immediately |

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
  Mount the module away from the ESP and voltage regulators (see [Mounting location](#mounting-location)),
  and correct any remaining offset with `temperature_offset` (see [Calibration](#calibration)).
  Do **not** use a `filters: - offset:` on the temperature sensor for this: filters only change the published
  temperature, not the calculated values.

- **Smoothing noisy light readings:**

  ```yaml
  illuminance:
    name: "GY39 Illuminance"
    filters:
      - sliding_window_moving_average:
          window_size: 5
          send_every: 1
  ```

- **Altitude** is calculated by the module from a fixed 1013.25 hPa reference, in whole metres,
  and drifts with the weather by 80–250 m. Use `sea_level_pressure` and the pressure trends instead.

---

## Mounting location

Where the module is mounted affects the readings more than any formula. General rules:

| Factor | Effect | What to do |
|---|---|---|
| **Direct sunlight** | The sensor heats up by several °C, so temperature reads high and RH reads low | Put it in a **radiation shield**: a white, louvered, ventilated housing (like a Stevenson screen) |
| **Rain and splashing water** | Wet sensor: RH stuck at 100 %, corrosion | Keep it under a roof, with the sensor opening facing **down**. Never seal it airtight. A PTFE vent membrane is fine |
| **Heat sources** | Self-heating or local warm air | Keep it ≥ 5 cm from the ESP and its regulator (put the module on a cable). Stay away from water heaters, dryers and lamps |
| **Directly under a ceiling** | Warm air collects there (stratification) and a sunlit roof slab radiates heat | Hang it **20–30 cm below** the ceiling, not touching it |
| **Long periods of RH > 90 % or condensation** | The BME280 can read high for hours afterwards and drift over time | Ventilated housing, away from steam sources |
| **Light sensor orientation** | The MAX44009 sits on the top side of the board. Facing down, it measures reflected light | Use `illuminance` as a **relative** brightness indicator, or mount a second module facing the sky |
| **Cable length** | I2C fails on long wires | UART at 9600 baud works over several metres. Keep I2C under about 30–50 cm |

Values that depend only on the **water content** of the air (dew point, mixing ratio)
and pressure-based values are hardly affected by heating of the sensor. See [Which values to trust](#which-values-to-trust).

**`station_altitude`** is the height of the **sensor**, not of the ground: ground elevation + floor height.
For example, ground 20 m + 4th floor (about 3 m per floor, so 9 m) + 2.5 m below the ceiling gives about 31 m.
An error of 1 m changes the sea-level pressure by only about 0.12 hPa.

---

## Calibration

ESPHome `filters` on `temperature` / `humidity` only change the published values.
The calculated values (dew point, absolute humidity, …) use the raw readings.
So this component has built-in calibration options that are applied **before** every calculation:

```yaml
    temperature_offset: -0.8      # °C
    humidity_calibration:         # 'measured -> true', 1 point = offset, 2 points = linear
      - 34.0 -> 32.8
      - 77.5 -> 75.3
    pressure_offset: 0.0          # hPa
```

**Order of operations (physically consistent):**

1. `humidity_calibration` is applied to the raw RH. This RH is measured at the sensor's own temperature.
2. The vapour pressure *e* is calculated from the raw temperature and the calibrated RH.
3. `temperature_offset` is applied.
4. RH is **re-evaluated at the corrected temperature with *e* kept constant**.
   A warm sensor does not change how much water is in the air.

So a `temperature_offset` that corrects self-heating also corrects RH, and leaves dew point and mixing ratio unchanged.
Example: the air is 25 °C / 60 %, and the sensor heated to 27 °C reads 53.3 %.
With `temperature_offset: -2` the component publishes 25.00 °C / 60.00 %.

**Humidity: saturated salt test (cheap and reliable)**

1. Put a few spoons of salt in a small cup and add only enough water to make a damp slurry. Some salt must stay undissolved.
2. Put the cup and the module (not the ESP) in an airtight box, at a stable room temperature away from sunlight.
3. Wait 6–12 hours, then note the RH reading.
4. Repeat with a second salt for a two-point calibration.

| Salt | Equilibrium RH at 25 °C (Greenspan 1977) |
|---|---|
| Sodium chloride, NaCl (table salt) | **75.3 %** |
| Magnesium chloride, MgCl₂ | **32.8 %** |

Then set, for example, `humidity_calibration: ["34.0 -> 32.8", "77.5 -> 75.3"]`.
Do this test **before** setting `temperature_offset`, with `temperature_offset` at 0.

**Temperature:** place a trusted thermometer next to the module (in the shade, with air movement),
compare the readings over a few hours, and set `temperature_offset` to the average difference.

**Pressure:** compare `sea_level_pressure` with a nearby official weather station over several days
(only at times with little weather change). Set `pressure_offset` to the average difference.
The trends do not need this, because a constant offset cancels out.

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

## Calculated values

All values are calculated on the ESP in double precision from the (calibrated) temperature, RH and pressure
of the same reading. Wherever possible they use reference-grade equations instead of approximations.
Every equation was checked on a host computer against an independent implementation.

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

### Mixing ratio

$$w = \varepsilon\,\frac{e}{p-e}, \qquad \varepsilon = M_w/M_a = 0.621945$$

(ASHRAE Fundamentals), with *e* calculated exactly as for absolute humidity. Unit g/kg of dry air.
Unlike absolute humidity, the mixing ratio does not change when the same air is heated, cooled or compressed.
This makes it the fairest way to compare indoor and outdoor air, or two rooms.
Example: 25 °C, 60 % RH, 1013.25 hPa → **11.95 g/kg**.
Accuracy: about ±8 % (from ±3 % RH and ±1 °C), and it is **not affected by self-heating**.

### Sea-level pressure

Reduction formula of the Deutscher Wetterdienst (DWD):

$$p_0 = p\cdot\exp\!\left(\frac{g\,h}{R_d\,T_h}\right),\qquad T_h = T + C_h\,E + \frac{a\,h}{2}$$

| Symbol | Value |
|---|---|
| $g$ | 9.80665 m/s² |
| $R_d$ | 287.05 J/(kg·K) |
| $a$ | 0.0065 K/m (mean temperature lapse rate) |
| $C_h$ | 0.12 K/hPa (humidity correction) |
| $E$ | vapour pressure in hPa (calculated exactly) |
| $h$ | `station_altitude` |

Unlike the simple barometric formula, it corrects for humidity. The difference is 0.13 hPa at 120 m.
Accuracy: ±1 hPa (BME280 absolute accuracy), plus 0.12 hPa per metre of error in `station_altitude`.
With a `pressure_offset` determined against an official station, the result can be compared with
weather service values to a few tenths of a hPa.

### Pressure trends and tendency

The component keeps **10-minute means** of the station pressure for 24 hours
(145 values, about 0.6 kB RAM). It publishes, every 10 minutes:

- `pressure_trend_3h`: the latest 10-minute mean minus the one 3 h earlier. Available after **about 3 h 10 min**.
- `pressure_trend_24h`: the same over 24 h. Available after **about 24 h 10 min**.
- `pressure_tendency`: a text description of the 3 h change, using the UK Met Office / WMO terms.

| 3 h change (hPa) | `en` | `zh-TW` |
|---|---|---|
| < 0.1 | Steady | 平穩 |
| 0.1 – 1.5 | Rising / Falling slowly | 緩慢上升／下降 |
| 1.6 – 3.5 | Rising / Falling | 上升／下降 |
| 3.6 – 6.0 | Rising / Falling quickly | 快速上升／下降 |
| > 6.0 | Rising / Falling very rapidly | 非常快速上升／下降 |

Before enough history exists, the trends are `NaN` and the text is "Collecting data". The history is kept
in RAM only, so it restarts after a reboot. Bins without any reading (for example during a Wi-Fi outage
or an `update_interval` longer than 10 min) are stored as `NaN` and never interpolated.

Accuracy: trends use differences, so the absolute error cancels out. What remains is the BME280
**relative accuracy of ±0.12 hPa**, which makes the trends the most accurate output of the module.

**Daily pressure cycle (important in the tropics and subtropics, including Taiwan):**
even in perfectly stable weather, pressure rises and falls about ±1 hPa twice a day
(semidiurnal atmospheric tide). Maxima are around 10:00 and 22:00, minima around 04:00 and 16:00 local time.
So a 3 h change of up to 1–2 hPa can be completely normal.
The **24 h trend** compares the same time of day and cancels this cycle.
Practical thresholds for weather alerts:

- 3 h fall > 3 hPa, or 24 h fall > 5 hPa → a weather system is approaching
- 3 h fall > 6 hPa → strong system (for example a typhoon)

A pressure trend is an indicator, not a forecast. Convective showers, for example, often show no pressure signal.
Don't move the sensor: 10 m of height is about 1.2 hPa.

### Which values to trust

The equations add negligible error. What matters is how the BME280's errors
(±1 °C, ±3 % RH, ±1 hPa absolute, ±0.12 hPa relative) propagate, and how the value reacts to
**self-heating** of the sensor (by the ESP, the sun or a warm ceiling).

Propagated uncertainty at 25 °C, 60 % RH, 1013 hPa, and the error caused by +2 K of self-heating:

| Value | Uncertainty (spec) | Error from +2 K self-heating | Reference value |
|---|---|---|---|
| `pressure_trend_3h` / `_24h` | ±0.12 hPa (relative) | none | 🟢 high |
| `pressure`, `sea_level_pressure` | ±1 hPa | none | 🟢 high |
| `dew_point` | ±1.2 K | **0** | 🟢 high |
| `mixing_ratio` | ±8 % | **0** | 🟢 high |
| `absolute_humidity` | ±7.5 % | −0.7 % | 🟢 high |
| `temperature` | ±1 °C | **+2 K** | 🟠 only after good mounting and calibration |
| `humidity` (RH) | ±3 % RH | **−6.7 % RH** | 🟠 only after good mounting and calibration |
| `altitude` (from the module) | ±8 m, drifts 80–250 m with the weather | none | 🔴 low |

Why are dew point and mixing ratio immune to self-heating? The BME280 measures temperature and humidity on the
same chip. When the chip is warmer than the air, it reads a higher temperature and a correspondingly lower RH,
but the vapour pressure *e* calculated from both is unchanged. Every value that depends only on *e* stays correct.

Not provided on purpose:

- **VPD**: +31 % error from 2 K of self-heating, and ±31 % at high RH.
- **Heat index**: an empirical formula that amplifies a 1 °C error to more than 3 °C.
- **Wet-bulb temperature, enthalpy, Humidex**: all sensitive to self-heating.
- **PPFD from lux**: needs the light spectrum.

The uncertainties assume independent errors within the BME280's 20–80 % RH range, and are theoretical.
They have not been verified against reference instruments.

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
