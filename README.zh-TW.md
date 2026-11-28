# ESPHome GY-39 外部元件

[English](README.md) | **繁體中文**

這是給 **GY-39** 環境感測模組用的 [ESPHome](https://esphome.io) 外部元件（external component）。
GY-39 由 MAX44009 照度感測器、BME280 溫濕度/氣壓感測器，加上板上 MCU 組成。
本元件同時支援 **UART** 與 **I2C** 兩種介面。

**模組直接量測**

| 感測項目      | 單位 | 說明                          |
|---------------|------|-------------------------------|
| `illuminance` | lx   | 照度（0.045 – 188,000 lx）    |
| `temperature` | °C   | 溫度                          |
| `humidity`    | %    | 相對濕度                      |
| `pressure`    | hPa  | 氣壓（測站氣壓）              |
| `altitude`    | m    | 模組推算的海拔，只有整數公尺。*不建議使用*，原因見[數值參考價值](#數值參考價值) |

**本元件計算**（說明見[計算值](#計算值)）

| 感測項目             | 單位 | 說明 |
|----------------------|------|------|
| `dew_point`          | °C   | 露點（WMO 定義，以液態水為基準） |
| `absolute_humidity`  | g/m³ | 絕對濕度（水氣密度） |
| `mixing_ratio`       | g/kg | 混合比：每公斤乾空氣含有的水氣質量 |
| `sea_level_pressure` | hPa  | 換算到海平面的氣壓（需要設定 `station_altitude`） |
| `pressure_trend_3h`  | hPa  | 過去 3 小時的氣壓變化（WMO 標準） |
| `pressure_trend_24h` | hPa  | 過去 24 小時的氣壓變化（可以消除每日的氣壓起伏） |
| `pressure_tendency`  | 文字 | 氣壓趨勢的文字描述，例如「緩慢下降」（text sensor） |

每一項都可以選擇要不要啟用，也都支援 ESPHome 一般 [Sensor](https://esphome.io/components/sensor/) 的設定（`name`、`id`、`filters` 等）。
計算值只有在設定檔中啟用時才會計算。

> **實體名稱要包含英文或數字**：較新版的 ESPHome 會把實體名稱轉成 ASCII 的 ID，中文字會全部變成 `_`。
> 如果名稱只有中文，例如 `溫度` 和 `濕度`，兩個都會變成同一個 ID `__`，設定驗證就會失敗。
> 所以本文件的範例都用「中文 + 英文」的名稱，例如 `溫度 Temperature`。
> 你也可以用英文命名，之後再到 Home Assistant 裡改成中文顯示名稱。

---

## 目錄

- [安裝](#安裝)
- [Package 參考](#package-參考)
- [手動設定](#手動設定)
- [UART 與 I2C 怎麼選](#uart-與-i2c-怎麼選)
- [接線](#接線)
- [UART 使用範例](#uart-使用範例)
- [I2C 使用範例](#i2c-使用範例)
- [設定選項](#設定選項)
- [使用建議](#使用建議)
- [安裝位置](#安裝位置)
- [校正](#校正)
- [疑難排解](#疑難排解)
- [計算值](#計算值)
  - [數值參考價值](#數值參考價值)
- [通訊協定說明](#通訊協定說明)

---

## 安裝

### 方法一：用 Package 匯入（建議）

只要一段 `packages:`，就會加上元件、匯流排（UART）和所有[建議的感測器](#手動設定)。
你只需要在 `vars:` 底下設定幾個變數。

**UART 模式**（出廠預設，S0 懸空）：package 會自己建立 UART 匯流排。

```yaml
packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-uart.yaml
        vars:
          gy39_station_altitude: "31"     # 必填：感測器的海拔（m）
          gy39_tx_pin: GPIO17             # ESP TX -> GY-39 DR
          gy39_rx_pin: GPIO16             # ESP RX <- GY-39 CT
          gy39_tendency_language: zh-TW   # 趨勢文字用中文
```

**I2C 模式**（S0 接 GND）：package 會使用你設定檔裡的 `i2c:` 匯流排。

```yaml
i2c:
  sda: GPIO4                              # 接 GY-39 的 DR
  scl: GPIO5                              # 接 GY-39 的 CT
  frequency: 50kHz

packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-i2c.yaml
        vars:
          gy39_station_altitude: "31"     # 必填
          gy39_tendency_language: zh-TW
```

- 完整的裝置設定：[UART 使用範例](#uart-使用範例)、[I2C 使用範例](#i2c-使用範例)，或 [`examples/`](examples/) 資料夾裡的檔案。
- 所有變數、如何修改或拿掉感測器、如何接多顆模組：請看 [Package 參考](#package-參考)。

### 方法二：只載入元件

如果想自己撰寫感測器設定（見[手動設定](#手動設定)）：

```yaml
external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]
```

**固定版本：** 把 `main`（package 的 `ref`、`gy39_ref`，或上面的 `@main`）換成 tag 或 commit hash。

---

## Package 參考

[方法一](#方法一用-package-匯入建議)的詳細說明。

| Package | 匯流排 |
|---|---|
| `packages/gy39-uart.yaml` | 自己建立 UART 匯流排 `${gy39_id}_uart` |
| `packages/gy39-i2c.yaml` | 使用**主設定檔**裡的 `i2c:` 匯流排（請在主設定檔定義，建議 `frequency: 50kHz`） |

兩種 package 都會加上外部元件和所有建議的感測器（完整清單見[手動設定](#手動設定)）。
完整範例：[`examples/gy39-uart.yaml`](examples/gy39-uart.yaml)、[`examples/gy39-i2c.yaml`](examples/gy39-i2c.yaml)。

**變數**請寫在 `vars:` 底下。注意：package 內建的 `defaults:` 優先權比主設定檔最上層的 `substitutions:` 高，
所以用 `substitutions:` 只能設定沒有預設值的 `gy39_station_altitude`，其他變數一定要用 `vars:`。

| 變數 | 適用 | 預設值 | 說明 |
|---|---|---|---|
| `gy39_station_altitude` | 兩者 | **必填** | 感測器的海拔（m） |
| `gy39_id` | 兩者 | `gy39` | 所有 ID 的前綴（`gy39_sensor`、`gy39_temperature` 等）。接多顆模組時，每顆要用不同的值 |
| `gy39_name_prefix` | 兩者 | `""` | 實體名稱前綴，例如 `"Balcony "` 或 `"陽台 "`（注意結尾的空格）。接多顆模組時，前綴要有**英文或數字上的差異**（例如 `"室內 Indoor "`、`"陽台 Balcony "`），因為中文字在 ESPHome 內部的 ID 裡都會變成 `_`，只差中文會被判定為重複 |
| `gy39_update_interval` | 兩者 | `60s` | 更新間隔 |
| `gy39_tendency_language` | 兩者 | `en` | 趨勢文字的語言：`en` 或 `zh-TW` |
| `gy39_temperature_offset` | 兩者 | `0` | 溫度修正（°C），見[校正](#校正) |
| `gy39_pressure_offset` | 兩者 | `0` | 氣壓修正（hPa） |
| `gy39_failure_timeout` | 兩者 | `60s` | 見[設定選項](#設定選項)中的 `failure_timeout` |
| `gy39_ref` | 兩者 | `main` | 元件的 git 版本，請設成和 package 的 `ref` 相同 |
| `gy39_tx_pin` / `gy39_rx_pin` | UART | `GPIO17` / `GPIO16` | ESP TX → GY-39 DR、ESP RX ← GY-39 CT |
| `gy39_baud_rate` | UART | `9600` | UART 鮑率 |
| `gy39_address` | I2C | `0x5B` | I2C 位址 |

**修改 package 的內容。** 感測器平台本身的 ID 是 `${gy39_id}_sensor`。不能只叫 `gy39`，因為這個名稱被元件本身佔用了。
用 `!extend` 修改設定，用 `!remove` 拿掉不要的感測器：

```yaml
sensor:
  - id: !extend gy39_sensor
    humidity_calibration: ["34.0 -> 32.8", "77.5 -> 75.3"]
    illuminance: !remove
    mixing_ratio: !remove
```

**多條 I2C 匯流排或多顆模組。** 用 `i2c_id` 指定匯流排，每顆模組要用不同的 `gy39_id`：

```yaml
i2c:
  - {id: bus_a, sda: GPIO21, scl: GPIO22, frequency: 50kHz}
  - {id: bus_b, sda: GPIO18, scl: GPIO19, frequency: 50kHz}
packages:
  indoor:
    url: https://github.com/ch155260/esphome-gy39
    files:
      - path: packages/gy39-i2c.yaml
        vars: {gy39_id: indoor, gy39_name_prefix: "室內 Indoor ", gy39_station_altitude: "31"}
  outdoor:
    url: https://github.com/ch155260/esphome-gy39
    files:
      - path: packages/gy39-i2c.yaml
        vars: {gy39_id: outdoor, gy39_name_prefix: "陽台 Balcony ", gy39_station_altitude: "31"}
sensor:
  - id: !extend indoor_sensor
    i2c_id: bus_a
  - id: !extend outdoor_sensor
    i2c_id: bus_b
```

ESP8266 如果用 UART package 接在 GPIO1/GPIO3，記得另外設定 `logger: baud_rate: 0`。

---

## 手動設定

不用 package 的話，先用 `external_components` 載入元件（見[安裝](#安裝)），再自己撰寫設定。
下面這段就是 package 幫你設定的內容，只包含參考價值高的數值（原因見[數值參考價值](#數值參考價值)）。
另外還需要 `uart:` 或 `i2c:` 匯流排。

```yaml
sensor:
  - platform: gy39
    interface: uart            # 或 i2c
    uart_id: gy39_uart
    update_interval: 60s
    station_altitude: 35       # 感測器的海拔（m）= 地面海拔 + 安裝高度
    tendency_language: zh-TW   # 趨勢文字用中文
    # 校正，請參考「校正」一節。會在所有計算之前套用。
    # temperature_offset: -0.8
    # humidity_calibration:
    #   - 34.0 -> 32.8
    #   - 77.5 -> 75.3

    # 直接量測
    temperature:
      name: "溫度 Temperature"
    humidity:
      name: "濕度 Humidity"
    pressure:
      name: "氣壓 Pressure"
    illuminance:
      name: "照度 Illuminance"

    # 計算值（不受自熱影響，參考價值高）
    dew_point:
      name: "露點 Dew Point"
    absolute_humidity:
      name: "絕對濕度 Absolute Humidity"
    mixing_ratio:
      name: "混合比 Mixing Ratio"
    sea_level_pressure:
      name: "海平面氣壓 Sea Level Pressure"
    pressure_trend_3h:
      name: "3小時氣壓變化 Pressure Trend 3h"
    pressure_trend_24h:
      name: "24小時氣壓變化 Pressure Trend 24h"
    pressure_tendency:
      name: "氣壓趨勢 Pressure Tendency"
```

---

## UART 與 I2C 怎麼選

模組用 **S0 腳**決定介面，而且只在**上電當下**讀取一次：

| 上電時的 S0          | 模式 | YAML 的 `interface:` |
|----------------------|------|----------------------|
| 懸空（不接）         | UART（出廠預設） | `uart`（預設值） |
| 接 **GND**           | I2C  | `i2c`                |

- `interface:` 只是告訴 ESPHome 要用哪種方式跟模組溝通，**一定要和 S0 的接法一致**。
- 改了 S0 之後，模組要**斷電再上電**。如果模組一直有供電，只重開 ESP 是沒有用的。
- 一顆模組一次只能用一種模式，因為 CT/DR 兩支腳是共用的。
  不過同一份設定裡，不同的模組可以混用不同模式。

| | UART | I2C |
|---|---|---|
| 接線 | 要佔用 2 支專用腳位（TX/RX） | 可以和其他感測器共用 I2C 匯流排 |
| 資料正確性 | 每一幀都會檢查校驗和 | 沒有校驗 |
| 設定難度 | 接上就能用 | S0 要接地，匯流排也要降速 |
| 接多顆模組 | 每顆各用一組 UART | 每條匯流排只能接一顆（位址固定 0x5B） |
| **適合誰** | **大多數人** | 腳位不夠用，或想共用現有的 I2C 匯流排 |

---

## 接線

| GY-39 腳位 | UART 模式                       | I2C 模式                  |
|------------|---------------------------------|---------------------------|
| VCC        | 3.3 V 或 5 V                    | 3.3 V 或 5 V              |
| GND        | GND                             | GND                       |
| CT         | 模組 **TX** → ESP 的 **RX** 腳  | **SCL**                   |
| DR         | 模組 **RX** → ESP 的 **TX** 腳  | **SDA**                   |
| S0         | 懸空                            | **接 GND（上電前接好）**  |

> UART 的 TX/RX 要**交叉接**：模組的 TX（CT）接 ESP 的 RX，模組的 RX（DR）接 ESP 的 TX。

---

## UART 使用範例

### ESP32

ESP32 有多組閒置的硬體 UART，設定最簡單。
接線：CT → GPIO16（ESP RX）、DR → GPIO17（ESP TX），S0 懸空。

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
          gy39_station_altitude: "35"     # 必填：感測器的海拔（m）
          gy39_tx_pin: GPIO17             # ESP TX -> GY-39 DR（模組 RX）
          gy39_rx_pin: GPIO16             # ESP RX <- GY-39 CT（模組 TX）
          gy39_tendency_language: zh-TW   # 趨勢文字用中文
```

### ESP8266

ESP8266 只有一組完整的硬體 UART（GPIO1/GPIO3），而且預設被 logger 佔用。
第一種做法是把它從 logger 讓出來：

```yaml
logger:
  baud_rate: 0                            # 把硬體 UART0 讓給感測器

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

第二種做法是保留序列埠 log，改用其他腳位的軟體序列埠：

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
          gy39_tx_pin: GPIO14             # D5，軟體序列埠
          gy39_rx_pin: GPIO12             # D6，軟體序列埠（9600 baud 沒問題）
```

**UART 模式運作方式：** 每到一次 `update_interval`，元件就送出「查詢一次」指令 `A5 53 F8`，
收到照度和 BME280 兩個回應幀之後發布數值。
如果模組處於連續輸出模式，多出來的幀會被忽略，所以仍然只會照 `update_interval` 更新。

---

## I2C 使用範例

1. 先把 **S0 接 GND**，再讓模組上電。
2. 在主設定檔定義 I2C 匯流排（I2C package 會使用它），頻率要調低。

```yaml
esphome:
  name: gy39-i2c
  friendly_name: GY39

esp8266:
  board: d1_mini

logger:

i2c:
  sda: GPIO4          # D2，接 GY-39 的 DR
  scl: GPIO5          # D1，接 GY-39 的 CT
  frequency: 50kHz    # 建議值，模組上的 MCU 回應比較慢
  scan: true          # 開機時 log 應該會掃到 0x5B

packages:
  gy39:
    url: https://github.com/ch155260/esphome-gy39
    ref: main
    refresh: 1d
    files:
      - path: packages/gy39-i2c.yaml
        vars:
          gy39_station_altitude: "35"     # 必填：感測器的海拔（m）
          gy39_name_prefix: "陽台 "       # 實體名稱前綴（選填）
          gy39_tendency_language: zh-TW   # 趨勢文字用中文
```

ESP32 可以用任意兩支 GPIO，例如 `sda: GPIO21`、`scl: GPIO22`，頻率一樣設 `frequency: 50kHz`。

**I2C 模式運作方式：** 每到一次 `update_interval`，元件用一次傳輸讀取暫存器 `0x00`–`0x0D`（共 14 bytes），
然後發布所有已啟用的感測值。

---

## 設定選項

| 選項              | 模式 | 必填 | 預設值  | 說明 |
|-------------------|------|------|---------|------|
| `interface`       | 全部 | 否   | `uart`  | `uart` 或 `i2c` |
| `uart_id`         | UART | 否\* | –       | `uart:` 匯流排的 ID（\*定義了多組 UART 時才需要） |
| `i2c_id`          | I2C  | 否\* | –       | `i2c:` 匯流排的 ID（\*定義了多條匯流排時才需要） |
| `address`         | I2C  | 否   | `0x5B`  | I2C 位址 |
| `update_interval` | 全部 | 否   | `60s`   | 讀取並發布數值的間隔 |
| `illuminance`     | 全部 | 否   | –       | 照度（lx） |
| `temperature`     | 全部 | 否   | –       | 溫度（°C） |
| `pressure`        | 全部 | 否   | –       | 氣壓（hPa） |
| `humidity`        | 全部 | 否   | –       | 濕度（%） |
| `altitude`        | 全部 | 否   | –       | 海拔（m），由模組提供 |
| `dew_point`       | 全部 | 否   | –       | 露點（°C），計算值 |
| `absolute_humidity` | 全部 | 否 | –       | 絕對濕度（g/m³），計算值 |
| `mixing_ratio`    | 全部 | 否   | –       | 混合比（g/kg），計算值 |
| `sea_level_pressure` | 全部 | 否 | –      | 海平面氣壓（hPa），計算值，需要 `station_altitude` |
| `station_altitude` | 全部 | 有 `sea_level_pressure` 時必填 | – | **感測器本身**的海拔，單位 m（−500 … 9000） |
| `pressure_trend_3h` | 全部 | 否 | –       | 3 小時氣壓變化（hPa） |
| `pressure_trend_24h` | 全部 | 否 | –      | 24 小時氣壓變化（hPa） |
| `pressure_tendency` | 全部 | 否 | –       | 氣壓趨勢文字（text sensor） |
| `tendency_language` | 全部 | 否 | `en`    | `pressure_tendency` 的語言：`en` 或 `zh-TW` |
| `temperature_offset` | 全部 | 否 | `0`    | 溫度修正量（°C），RH 會跟著重新計算，見[校正](#校正) |
| `humidity_calibration` | 全部 | 否 | –    | 1 或 2 個校正點，格式為 `量測值 -> 真值`（%RH） |
| `pressure_offset` | 全部 | 否   | `0`     | 氣壓修正量（hPa） |
| `failure_timeout` | 全部 | 否   | `60s`   | 超過這段時間都沒有成功讀取時，所有即時值和計算值會發布一次「未知」（NaN），趨勢文字顯示「無資料」。設 `0s` 表示立即 |

---

## 使用建議

- **沒有特別需求就用 UART**：它是出廠預設，不用額外接線，每一幀也都有校驗和檢查。
- **`update_interval`**：環境數據設 10 s – 60 s 就很夠用了。
  建議不要低於 2 s 左右，因為模組本身每秒只更新幾次。
- **I2C 速度**：建議 **50 kHz**，最多不要超過 100 kHz。模組上的 MCU 回應比較慢，速度太快容易讀取失敗。
- **I2C 上拉電阻**：大部分 GY-39 板子上已經有上拉電阻。
  如果線比較長，或匯流排上接了很多裝置，可以另外加 4.7 kΩ 上拉到 3.3 V。
- **供電**：板子上有穩壓 IC，3.3 V 和 5 V 都能用。
  訊號電位會跟著 VCC 走，所以**接 ESP 的時候建議用 3.3 V 供電**，讓訊號維持在 3.3 V。
- **溫度準確度**：BME280 會被附近的電路加熱而偏高。
  模組要裝在離 ESP 和穩壓 IC 遠一點的地方（見[安裝位置](#安裝位置)），剩下的偏差再用 `temperature_offset` 修正（見[校正](#校正)）。
  **不要**為了這個在溫度感測器上加 `filters: - offset:`：濾波器只會改變發布出去的溫度，不會影響各項計算值。

- **照度數值跳動時可以做平滑處理**：

  ```yaml
  illuminance:
    name: "GY39 照度 Illuminance"
    filters:
      - sliding_window_moving_average:
          window_size: 5
          send_every: 1
  ```

- **海拔**是模組用固定的 1013.25 hPa 當基準推算的，只有整數公尺，而且會跟著天氣飄動 80–250 m。
  建議改用 `sea_level_pressure` 和氣壓趨勢。

---

## 安裝位置

模組裝在哪裡，對讀數的影響比任何公式都大。一般原則如下：

| 因素 | 影響 | 建議做法 |
|---|---|---|
| **陽光直射** | 感測器會升溫好幾 °C，溫度偏高、RH 偏低 | 裝在**防輻射罩**裡：白色、百葉式、通風的外殼（類似氣象站的百葉箱） |
| **雨水、噴濺** | 感測器受潮，RH 卡在 100 %，還會腐蝕 | 裝在屋簷下，感測器開口**朝下**。不要完全密封；用 PTFE 透氣膜可以 |
| **熱源** | 自熱或局部熱空氣 | 和 ESP、穩壓 IC 距離 ≥ 5 cm（用線把模組拉出來）。遠離熱水器、烘衣機、燈具 |
| **緊貼天花板** | 熱空氣會往上累積（溫度分層），被曬熱的樓板還會輻射熱 | 從天花板往下吊 **20–30 cm**，不要貼著天花板 |
| **長時間 RH > 90 % 或結露** | BME280 之後幾個小時可能讀值偏高，長期會漂移 | 用通風的外殼，遠離蒸氣來源 |
| **照度感測器的方向** | MAX44009 在板子正面，朝下時量到的是反射光 | 把 `illuminance` 當**相對**亮度指標使用，或另外裝一顆朝向天空的模組 |
| **線長** | 線太長時 I2C 會失效 | UART 9600 baud 拉好幾公尺都沒問題；I2C 建議控制在 30–50 cm 以內 |

只取決於**空氣含水量**的數值（露點、混合比），以及以氣壓為基礎的數值，幾乎不受感測器發熱影響，詳見[數值參考價值](#數值參考價值)。

**`station_altitude`** 指的是**感測器**的海拔，不是地面的海拔：要用地面海拔加上樓層高度。
例如地面海拔 20 m，住 4 樓（每層約 3 m，地板高 9 m），再加上天花板下方 2.5 m 的安裝高度，大約是 31 m。
這個值每差 1 m，海平面氣壓只差約 0.12 hPa。

---

## 校正

ESPHome 在 `temperature`、`humidity` 上設定的 `filters`，只會改變發布出去的數值。
計算值（露點、絕對濕度等）用的是原始讀數，不會被這些濾波器校正到。
所以本元件內建了校正選項，會在**所有計算之前**套用：

```yaml
    temperature_offset: -0.8      # °C
    humidity_calibration:         # '量測值 -> 真值'；1 點 = 只修正偏移，2 點 = 線性校正
      - 34.0 -> 32.8
      - 77.5 -> 75.3
    pressure_offset: 0.0          # hPa
```

**處理順序（符合物理原理）：**

1. 先對原始 RH 套用 `humidity_calibration`。這時的 RH 是在感測器自己的溫度下量到的。
2. 用原始溫度和校正後的 RH 算出水氣壓 *e*。
3. 套用 `temperature_offset`。
4. **在 *e* 不變的前提下，用修正後的溫度重新計算 RH**。感測器發熱，並不會改變空氣中的含水量。

所以用 `temperature_offset` 修正自熱時，RH 也會一起被正確修正，而露點和混合比維持不變。
例如：空氣實際是 25 °C、60 %，感測器被加熱到 27 °C，讀到 53.3 %。
設定 `temperature_offset: -2` 之後，元件會發布 25.00 °C、60.00 %。

**濕度：飽和鹽水測試（便宜又可靠）**

1. 在小杯子裡放幾匙鹽，只加一點水攪成濕濕的泥狀。杯底一定要留著沒溶解的鹽。
2. 把杯子和模組（ESP 不要放進去）一起放進密封盒，擺在室溫穩定、照不到陽光的地方。
3. 等 6–12 小時，記下 RH 讀數。
4. 換另一種鹽再做一次，就可以做兩點校正。

| 鹽類 | 25 °C 時的平衡濕度（Greenspan 1977） |
|---|---|
| 氯化鈉 NaCl（食鹽） | **75.3 %** |
| 氯化鎂 MgCl₂ | **32.8 %** |

然後設定，例如 `humidity_calibration: ["34.0 -> 32.8", "77.5 -> 75.3"]`。
這個測試要在設定 `temperature_offset` **之前**做，做的時候 `temperature_offset` 要保持 0。

**溫度：** 在模組旁邊放一支可信的溫度計（放在陰涼、有空氣流動的地方），
比對幾個小時，把 `temperature_offset` 設成平均差值。

**氣壓：** 連續幾天拿 `sea_level_pressure` 和附近氣象站的官方數值比對（挑天氣變化不大的時段），
把 `pressure_offset` 設成平均差值。氣壓趨勢不需要這個校正，因為固定的偏差在相減時會抵消。

---

## 疑難排解

| 狀況 | 可能原因／解法 |
|---|---|
| UART：`Timeout waiting for GY-39 response` | TX/RX 沒有交叉接、`baud_rate` 設錯、logger 佔用了 UART0（ESP8266），或是 S0 接了地（變成 I2C 模式） |
| UART：`Checksum mismatch` | 雜訊干擾或鮑率不一致。把線縮短，並確認有共地 |
| UART：改過鮑率之後收不到資料 | 如果之前送過 115200 的指令（`A5 AF 54`），要把 `baud_rate` 改成 `115200` |
| I2C：`Communication with GY-39 failed` | S0 沒有在**上電前**接地。把模組斷電重開，並確認 `scan: true` 有掃到 0x5B |
| I2C：偶爾讀取失敗 | 把 `frequency` 再調低（例如 50 kHz 或 20 kHz）、縮短線長、加上拉電阻 |
| 溫度偏高 | 自熱問題，請參考[使用建議](#使用建議) |

想看原始數值的話，可以打開詳細 log：

```yaml
logger:
  level: DEBUG
  logs:
    gy39: DEBUG
```

---

## 計算值

所有計算值都在 ESP 上用倍精度（double）計算，輸入是同一次讀取中（校正後的）溫度、相對濕度和氣壓。
只要有參考等級的公式，就不用近似式。
每一條公式都在主機上和另一個獨立的實作比對過。

### 絕對濕度

`absolute_humidity` 是每立方公尺空氣中水氣的質量（g/m³）。
模組本身不會直接量這個值，而是由元件在 ESP 上用倍精度（double）計算，
輸入是同一次讀取中模組回報的溫度、相對濕度和氣壓。
這裡沒有用教科書常見的 Magnus 近似式，而是採用參考等級的公式：

**1. 實際水氣壓**（採用 WMO 對相對濕度的定義，以液態水為基準）：

$$e = \frac{RH}{100}\cdot f(p,t)\cdot e_w(T)$$

**2. 水氣密度**（真實氣體形式）：

$$\rho_v = \frac{e \cdot M_w}{Z \cdot R \cdot T}$$

| 項目 | 採用公式 | 說明 |
|---|---|---|
| $e_w(T)$，T ≥ 0.01 °C | **Wagner & Pruss (2002)**，IAPWS 飽和蒸氣壓方程式 | 和 IAPWS-95 的偏差 < 0.01 % |
| $e_w(T)$，T < 0.01 °C | **Murphy & Koop (2005)**，過冷液態水 | 兩條公式在三相點都是 611.657 Pa，曲線是連續的 |
| $f(p,t)$ | **Buck (1996)** 增強因子 $1+10^{-4}[7.2+P(0.0320+5.9\cdot10^{-6}t^2)]$ | 修正「空氣中的水氣」和「純水氣」的差異（1013 hPa 時約 +0.4 %） |
| $Z$ | **CIPM-2007** 濕空氣壓縮因子（Picard et al., *Metrologia* 45, 2008） | 室溫附近約 0.9996 |
| $R$、$M_w$ | 8.314462618 J/(mol·K)（CODATA 2018）、18.01528 g/mol | |

**輸入處理**
- RH 會限制在 0–100 %。
- 只有在 −45…85 °C、300…1100 hPa（BME280 的量測範圍）內才會發布數值。
  超出範圍時發布 `NaN`，並在 log 印出警告。
- 計算時用的是實測氣壓，所以在高海拔地區結果一樣正確。

**準確度。** 公式本身帶來的誤差小於 0.1 %，真正的限制是 BME280 感測器
（典型誤差 ±3 % RH、±1 °C）。在 25 °C 時，溫度每差 1 °C，絕對濕度大約差 5.6 %；
相對濕度的誤差則是等比例傳遞過來。所以在 50 % RH 時，整體大約是 **±8 %**（兩項誤差取平方和開根號）。
想讓數值更準，最重要的是讓模組遠離熱源，或者用 `offset` 濾波器校正溫度。

| 驗算（1013.25 hPa） | 結果 |
|---|---|
| 20 °C、50 % RH | 8.68 g/m³ |
| 30 °C、100 % RH | 30.50 g/m³ |
| 0 °C、100 % RH | 4.87 g/m³ |
| −10 °C、80 % RH | 1.90 g/m³ |

### 露點

`dew_point` 採用 WMO 的定義：在氣壓和水氣含量都不變的情況下，空氣要冷卻到幾度，
才會**相對於液態水**達到飽和。程式求的是下面這個方程式的精確解：

$$f(p,T_d)\cdot e_w(T_d) = e$$

其中 $e_w$（Wagner & Pruss / Murphy & Koop）和 $f$（Buck）跟上面的絕對濕度用的是同一套。
這個方程式用數值方法求解：先用 Magnus 公式估一個初始值，再做牛頓迭代，
並且在 [−100 °C, t] 區間內用二分法保護，避免發散。收斂到 < 10⁻⁶ K，通常 3–4 次就收斂。
最終結果**不是**用 Magnus 近似式算的。
和另一個獨立的二分法求解器比對（−40…85 °C、5…100 % RH、700…1100 hPa，共 1560 點），
最大差異是 9.3 × 10⁻⁷ K。

- RH = 100 %（或更高）時，露點就等於氣溫。
- RH = 0 %，或露點低於 −100 °C 時，發布 `NaN`。
- 0 °C 以下的結果是依 WMO 定義、對**過冷水**的露點。它不是霜點，霜點會比較高。

**準確度。** 跟絕對濕度一樣，真正的限制是感測器本身：

| 條件 | ±3 % RH | ±1 °C | 合成誤差（RSS） |
|---|---|---|---|
| 20 °C、50 % RH（Td = 9.28 °C） | ±0.89 K | ±0.92 K | **±1.3 K** |
| 25 °C、60 % RH（Td = 16.71 °C） | ±0.79 K | ±0.94 K | **±1.2 K** |
| 0 °C、80 % RH（Td = −3.03 °C） | ±0.50 K | ±0.98 K | **±1.1 K** |

| 驗算（1013.25 hPa） | 露點 |
|---|---|
| 20 °C、50 % RH | 9.28 °C |
| 25 °C、60 % RH | 16.71 °C |
| 30 °C、80 % RH | 26.17 °C |
| 35 °C、30 % RH | 14.85 °C |
| 0 °C、50 % RH | −9.18 °C |
| −10 °C、80 % RH | −12.79 °C |

---

### 混合比

$$w = \varepsilon\,\frac{e}{p-e}, \qquad \varepsilon = M_w/M_a = 0.621945$$

係數取自 ASHRAE Fundamentals，*e* 的算法和絕對濕度完全相同，單位是每公斤乾空氣含多少克水氣（g/kg）。
絕對濕度會隨空氣加熱、冷卻或壓縮而改變，混合比不會，所以拿來比較室內外或不同房間的空氣最公平。
例如：25 °C、60 % RH、1013.25 hPa → **11.95 g/kg**。
準確度約 ±8 %（來自 ±3 % RH 和 ±1 °C），而且**不受自熱影響**。

### 海平面氣壓

採用德國氣象局（DWD）的海平面氣壓換算公式：

$$p_0 = p\cdot\exp\!\left(\frac{g\,h}{R_d\,T_h}\right),\qquad T_h = T + C_h\,E + \frac{a\,h}{2}$$

| 符號 | 數值 |
|---|---|
| $g$ | 9.80665 m/s² |
| $R_d$ | 287.05 J/(kg·K) |
| $a$ | 0.0065 K/m（平均氣溫垂直遞減率） |
| $C_h$ | 0.12 K/hPa（濕度修正） |
| $E$ | 水氣壓（hPa，精確計算） |
| $h$ | `station_altitude` |

和一般的簡易氣壓公式相比，這條公式多了濕度修正；在 120 m 時兩者差 0.13 hPa。
準確度是 ±1 hPa（BME280 的絕對精度），另外 `station_altitude` 每差 1 m 會再多 0.12 hPa。
如果用官方氣象站的數值定出 `pressure_offset`，結果可以和氣象單位的數值對到零點幾 hPa。

### 氣壓趨勢

元件會把測站氣壓整理成 **10 分鐘平均值**，在記憶體中保留 24 小時
（145 筆，約 0.6 kB RAM）。每 10 分鐘發布一次以下數值：

- `pressure_trend_3h`：最新的 10 分鐘平均值，減去 3 小時前的那一筆。開機後**約 3 小時 10 分**開始有數值。
- `pressure_trend_24h`：同樣的方式，但比較 24 小時前。開機後**約 24 小時 10 分**開始有數值。
- `pressure_tendency`：3 小時變化的文字描述，採用英國氣象局（UK Met Office）／WMO 的分級用語。

| 3 小時變化（hPa） | `en` | `zh-TW` |
|---|---|---|
| < 0.1 | Steady | 平穩 |
| 0.1 – 1.5 | Rising / Falling slowly | 緩慢上升／下降 |
| 1.6 – 3.5 | Rising / Falling | 上升／下降 |
| 3.6 – 6.0 | Rising / Falling quickly | 快速上升／下降 |
| > 6.0 | Rising / Falling very rapidly | 非常快速上升／下降 |

紀錄還不夠時，趨勢值會是 `NaN`，文字顯示「資料收集中」。紀錄只存在 RAM 裡，所以重開機後會重新累積。
某個 10 分鐘區間如果完全沒有讀數（例如 Wi-Fi 斷線，或 `update_interval` 設得比 10 分鐘還長），
會記成 `NaN`，不會用內插去補。

準確度：趨勢是兩個值相減，絕對誤差會互相抵消，剩下的是 BME280 **±0.12 hPa 的相對精度**。
所以氣壓趨勢是這個模組最準確的輸出。

**每日氣壓起伏（熱帶和副熱帶很重要，台灣也包含在內）：**
就算天氣完全穩定，氣壓每天也會起伏兩次，幅度約 ±1 hPa（半日潮）。
地方時大約 10:00、22:00 最高，04:00、16:00 最低。
所以 3 小時內有 1–2 hPa 的變化，可能完全正常。
**24 小時趨勢**比較的是每天同一個時刻，可以消掉這個起伏。
實用的天氣警示門檻：

- 3 小時下降超過 3 hPa，或 24 小時下降超過 5 hPa → 有天氣系統正在接近
- 3 小時下降超過 6 hPa → 強烈天氣系統（例如颱風）

氣壓趨勢只是一個指標，不能取代天氣預報；例如午後雷陣雨常常沒有明顯的氣壓前兆。
裝好之後不要再移動感測器：高度每差 10 m，氣壓大約差 1.2 hPa。

### 數值參考價值

公式本身帶來的誤差可以忽略。真正重要的是：BME280 的誤差
（±1 °C、±3 % RH、絕對 ±1 hPa、相對 ±0.12 hPa）傳遞到各數值後有多大，
以及數值會不會受到感測器**自熱**（被 ESP、陽光或熱天花板加熱）影響。

下表是 25 °C、60 % RH、1013 hPa 時的誤差傳遞結果，以及感測器自熱 +2 K 時造成的誤差：

| 數值 | 不確定度（依規格） | 自熱 +2 K 造成的誤差 | 參考價值 |
|---|---|---|---|
| `pressure_trend_3h` / `_24h` | ±0.12 hPa（相對） | 無 | 🟢 高 |
| `pressure`、`sea_level_pressure` | ±1 hPa | 無 | 🟢 高 |
| `dew_point` | ±1.2 K | **0** | 🟢 高 |
| `mixing_ratio` | ±8 % | **0** | 🟢 高 |
| `absolute_humidity` | ±7.5 % | −0.7 % | 🟢 高 |
| `temperature` | ±1 °C | **+2 K** | 🟠 要安裝得當並校正後才可靠 |
| `humidity`（RH） | ±3 % RH | **−6.7 % RH** | 🟠 要安裝得當並校正後才可靠 |
| `altitude`（模組提供） | ±8 m，會隨天氣飄動 80–250 m | 無 | 🔴 低 |

為什麼露點和混合比不受自熱影響？BME280 的溫度和濕度是在同一顆晶片上量的。
晶片比空氣熱的時候，溫度會讀高、RH 會相對讀低，但用兩者算出的水氣壓 *e* 不變；
只取決於 *e* 的數值，就都還是正確的。

刻意不提供的數值：

- **VPD**：自熱 2 K 就會偏 +31 %，高濕時誤差也有 ±31 %。
- **熱指數**：經驗公式，會把 1 °C 的誤差放大成 3 °C 以上。
- **濕球溫度、焓值、Humidex**：都會受自熱影響。
- **照度換算 PPFD**：需要知道光源的光譜。

上表的不確定度假設各項誤差互相獨立，而且只適用於 BME280 的 20–80 % RH 範圍。
這些都是理論推算，沒有用標準儀器實際驗證過。

---

## 通訊協定說明

**UART 資料幀**（模組 → 主機，校驗和 = 前面所有 bytes 相加後取低 8 位）：

| 幀 | 內容 |
|---|---|
| 照度   | `5A 5A 15 04` LUX(4, ÷100) `SUM` |
| BME280 | `5A 5A 45 0A` TEMP(2, 有號, ÷100) PRESS(4, ÷100 Pa) HUM(2, ÷100) ALT(2, m) `SUM` |

**指令**（主機 → 模組）：`A5 53 F8` = 查詢一次照度和 BME280（本元件使用的指令）。

**I2C 暫存器**（位址 0x5B，big-endian）：`0x00–0x03` 照度×100、`0x04–0x05` 溫度×100、
`0x06–0x09` 氣壓 Pa×100、`0x0A–0x0B` 濕度×100、`0x0C–0x0D` 海拔 m。

---

## 授權

[MIT](LICENSE)
