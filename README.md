# ESPHome GY-39 External Component

ESPHome 自訂元件，支援 **GY-39** 環境感測模組（MAX44009 照度 + BME280 溫濕度/氣壓，板上 MCU 輸出）。

| 量測項目 | 單位 | 說明 |
|---|---|---|
| `illuminance` | lx | 照度（0.045 ~ 188,000 lx） |
| `temperature` | °C | 溫度 |
| `pressure` | hPa | 氣壓 |
| `humidity` | % | 相對濕度 |
| `altitude` | m | 模組依氣壓推算的海拔 |

支援兩種通訊介面：

- **UART**（出廠預設，9600 8N1）：每個 `update_interval` 送出「查詢一次」指令 `A5 53 F8`，並解析 `5A 5A 15 ...`（照度）與 `5A 5A 45 ...`（BME280）回應幀，含校驗和檢查。模組若處於連續輸出模式也能正常運作（多餘的幀會被忽略）。
- **I2C**：上電前將 **S0 接 GND** 進入 I2C 模式，位址 `0x5B`，一次讀取暫存器 `0x00~0x0D`。

## 安裝

```yaml
external_components:
  - source: github://ch155260/esphome-gy39@main
    components: [gy39]
```

## 接線

| GY-39 | UART 模式 | I2C 模式 |
|---|---|---|
| VCC | 3.3V / 5V | 3.3V / 5V |
| GND | GND | GND |
| CT  | → ESP RX | SCL |
| DR  | → ESP TX | SDA |
| S0  | 懸空 | **接 GND（上電前）** |

## UART 範例

```yaml
uart:
  id: gy39_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

sensor:
  - platform: gy39
    interface: uart        # 預設值，可省略
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
```

## I2C 範例

```yaml
i2c:
  sda: GPIO4
  scl: GPIO5
  frequency: 50kHz   # 模組 MCU 較慢，建議 ≤100kHz

sensor:
  - platform: gy39
    interface: i2c
    address: 0x5B      # 預設值，可省略
    update_interval: 10s
    illuminance:
      name: "GY39 Illuminance"
    temperature:
      name: "GY39 Temperature"
```

## 設定選項

- **interface** (*選填*, 預設 `uart`)：`uart` 或 `i2c`
- **uart_id** (*UART 模式*)：UART bus ID
- **address** (*I2C 模式*, 預設 `0x5B`)
- **update_interval** (*選填*, 預設 `60s`)
- **illuminance / temperature / pressure / humidity / altitude** (*選填*)：皆支援所有標準 [Sensor](https://esphome.io/components/sensor/) 選項（`name`、`filters`、`id` …）

完整範例見 [`examples/`](examples/)。

## 疑難排解

- UART 無資料：確認 TX/RX 交叉接線、鮑率 9600（若曾用指令改為 115200，請同步修改 `baud_rate`）。
- Log 出現 `Timeout waiting for GY-39 response`：檢查接線與供電。
- I2C `Communication failed`：確認 S0 在**上電前**已接地，並降低 I2C 頻率。

## License

MIT
