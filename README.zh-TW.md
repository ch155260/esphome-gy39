# ESPHome GY-39 外部元件

[English](README.md) | **繁體中文**

這是給 **GY-39** 環境感測模組用的 [ESPHome](https://esphome.io) 外部元件（external component）。
GY-39 由 MAX44009 照度感測器、BME280 溫濕度/氣壓感測器，加上板上 MCU 組成。
本元件同時支援 **UART** 與 **I2C** 兩種介面。

| 感測項目      | 單位 | 說明                          |
|---------------|------|-------------------------------|
| `illuminance` | lx   | 照度（0.045 – 188,000 lx）    |
| `temperature` | °C   | 溫度                          |
| `pressure`    | hPa  | 氣壓                          |
| `humidity`    | %    | 相對濕度                      |
| `altitude`    | m    | 模組依氣壓推算的海拔          |
| `absolute_humidity` | g/m³ | 絕對濕度（水氣密度），由溫度、相對濕度、氣壓**計算**而得（[說明](#絕對濕度與露點)） |
| `dew_point`   | °C   | 露點，用同一組數據**計算**而得（[說明](#絕對濕度與露點)） |

每一項都可以選擇要不要啟用，也都支援 ESPHome 一般 [Sensor](https://esphome.io/components/sensor/) 的設定（`name`、`id`、`filters` 等）。

---

## 目錄

- [安裝](#安裝)
- [UART 與 I2C 怎麼選](#uart-與-i2c-怎麼選)
- [接線](#接線)
- [UART 使用範例](#uart-使用範例)
- [I2C 使用範例](#i2c-使用範例)
- [設定選項](#設定選項)
- [使用建議](#使用建議)
- [疑難排解](#疑難排解)
- [絕對濕度與露點](#絕對濕度與露點)
- [通訊協定說明](#通訊協定說明)

---

## 安裝

```yaml
external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]
```

想固定在某個版本的話，把 `@main` 換成 tag 或 commit hash。

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
  tx_pin: GPIO17   # 接 GY-39 的 DR（模組 RX）
  rx_pin: GPIO16   # 接 GY-39 的 CT（模組 TX）
  baud_rate: 9600  # 出廠預設

sensor:
  - platform: gy39
    interface: uart        # 預設值，可以省略
    uart_id: gy39_uart
    update_interval: 10s
    illuminance:
      name: "GY39 照度"
    temperature:
      name: "GY39 溫度"
    pressure:
      name: "GY39 氣壓"
    humidity:
      name: "GY39 濕度"
    altitude:
      name: "GY39 海拔"
    absolute_humidity:
      name: "GY39 絕對濕度"
    dew_point:
      name: "GY39 露點"
```

### ESP8266

ESP8266 只有一組完整的硬體 UART（GPIO1/GPIO3），而且預設被 logger 佔用。
有兩種做法，第一種是關掉 logger 的序列埠輸出：

```yaml
logger:
  baud_rate: 0          # 把硬體 UART0 讓給感測器

uart:
  id: gy39_uart
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600
```

第二種是保留序列埠 log，改用軟體序列埠：

```yaml
uart:
  id: gy39_uart
  tx_pin: GPIO14        # D5，軟體序列埠
  rx_pin: GPIO12        # D6，軟體序列埠
  baud_rate: 9600       # 軟體序列埠跑 9600 沒有問題
```

**UART 模式運作方式：** 每到一次 `update_interval`，元件就送出「查詢一次」指令 `A5 53 F8`，
收到照度和 BME280 兩個回應幀之後發布數值。
如果模組處於連續輸出模式，多出來的幀會被忽略，所以仍然只會照 `update_interval` 更新。

---

## I2C 使用範例

1. 先把 **S0 接 GND**，再讓模組上電。
2. I2C 匯流排的頻率要調低。

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
  sda: GPIO4        # D2，接 GY-39 的 DR
  scl: GPIO5        # D1，接 GY-39 的 CT
  frequency: 50kHz  # 建議值，模組上的 MCU 回應比較慢
  scan: true        # 開機時 log 應該會掃到 0x5B

sensor:
  - platform: gy39
    interface: i2c
    address: 0x5B   # 預設值，可以省略
    update_interval: 10s
    illuminance:
      name: "GY39 照度"
    temperature:
      name: "GY39 溫度"
    pressure:
      name: "GY39 氣壓"
    humidity:
      name: "GY39 濕度"
    altitude:
      name: "GY39 海拔"
    absolute_humidity:
      name: "GY39 絕對濕度"
    dew_point:
      name: "GY39 露點"
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
| `altitude`        | 全部 | 否   | –       | 海拔（m） |
| `absolute_humidity` | 全部 | 否 | –       | 絕對濕度（g/m³），計算值 |
| `dew_point`       | 全部 | 否   | –       | 露點（°C），計算值 |

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
  模組要裝在離 ESP 和穩壓 IC 遠一點的地方，或者用濾波器修正：

  ```yaml
  temperature:
    name: "GY39 溫度"
    filters:
      - offset: -1.5
  ```

- **照度數值跳動時可以做平滑處理**：

  ```yaml
  illuminance:
    name: "GY39 照度"
    filters:
      - sliding_window_moving_average:
          window_size: 5
          send_every: 1
  ```

- **海拔**是模組用標準海平面氣壓推算的，只能當作粗略參考，用不到的話可以不要啟用。

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

## 絕對濕度與露點

這兩個值都是在 ESP 上，用模組自己量到的溫度、相對濕度和氣壓計算出來的，用的是同一套參考等級公式。

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
