# T-Embed-PN532 引脚映射（V1.1 / 2026-06-08）

> 本文按 `hardware/T-Embed-PN532 V1.1.PDF` 整理。
>
> 需要先说明：虽然 PDF 文件名带 `PN532`，但 NFC 页面实际器件是 `ST25R3916`，本文以原理图器件与网络标注为准。
>
> 本文按模块分类整理。原理图里部分网络名明显沿用了旧设计命名，例如 `II2C_SDA/SCL`、`LORA_IO0/IO2`、`HPD_CS`、`SD_MODE`；文中会同时保留原网名并给出建议的板级宏名。

## 1. 主控与公共总线

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| 主 I2C SDA | `BOARD_I2C_SDA` | GPIO3 | 原理图 Page 1/2/3 | 原理图网名 `II2C_SDA`，板载充电/电量计/XL9555 与 QWIIC 共用 |
| 主 I2C SCL | `BOARD_I2C_SCL` | GPIO2 | 原理图 Page 1/2/3 | 原理图网名 `II2C_SCL`，板载充电/电量计/XL9555 与 QWIIC 共用 |
| 主 SPI SCK | `BOARD_SPI_SCK` | GPIO11 | 原理图 Page 2/3/4 | TF、LCD、NFC、CC1101 共用 |
| 主 SPI MOSI | `BOARD_SPI_MOSI` | GPIO9 | 原理图 Page 2/3/4 | TF、LCD、NFC、CC1101 共用 |
| 主 SPI MISO | `BOARD_SPI_MISO` | GPIO10 | 原理图 Page 2/3/4 | TF、LCD、NFC、CC1101 共用 |
| USB D- | `BOARD_USB_DM` | GPIO19 | 原理图 Page 1/2 | Type-C 直连 ESP32-S3 USB |
| USB D+ | `BOARD_USB_DP` | GPIO20 | 原理图 Page 1/2 | Type-C 直连 ESP32-S3 USB |
| 串口调试 TX | `BOARD_UART0_TXD` | `TXD0` | 原理图 Page 2 | 原理图只标了 `TXD0`，GPIO43 |
| 串口调试 RX | `BOARD_UART0_RXD` | `RXD0` | 原理图 Page 2 | 原理图只标了 `RXD0`，GPIO44 |

## 2. 按键与编码器

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| 用户按键 | `BOARD_KEY_PIN` | GPIO6 | 原理图 Page 2 | 原理图网名 `KEY_IO6`，独立按键 `K1` |
| 编码器 A 相 | `BOARD_ENC_A` | GPIO4 | 原理图 Page 2/4 | 原理图网名 `ESP_A` |
| 编码器 B 相 | `BOARD_ENC_B` | GPIO5 | 原理图 Page 2/4 | 原理图网名 `ESP_B` |
| 编码器按下 / BOOT | `BOARD_BOOT_PIN` | GPIO0 | 原理图 Page 2/4 | 既是编码器按键又是启动脚，低电平会影响启动模式 |
| 复位按键 | `BOARD_RST_EN` | `EN` | 原理图 Page 2 | 复位按键，不是普通 GPIO |

## 3. XL9555 扩展 IO

I2C 地址：`0x22`。

`XL9555` 的 `INT` 在原理图里没有接入 ESP32-S3，因此建议把 `BOARD_XL9555_INT` 视为 `-1`。

| XL9555 | 建议名 | 板级网络 | 方向 | 来源 | 备注 |
| --- | --- | --- | --- | --- | --- |
| P00 | `BOARD_XL9555_00_CC_SW1` | `CC_SW1` | 输出 | 原理图 Page 3 | CC1101 模块 `SW_1` |
| P01 | `BOARD_XL9555_01_CC_SW0` | `CC_SW0` | 输出 | 原理图 Page 3 | CC1101 模块 `SW_0` |
| P02 | `BOARD_XL9555_02_LCD_RST` | `LCD_RST` | 输出 | 原理图 Page 3/4 | LCD 复位 |
| P03 | `BOARD_XL9555_03_LOW_PWR_EN` | `LOW_PWR_EN` | 输出 | 原理图 Page 1/3 | 低功耗 3.3 V 电源域使能，`HIGH` 上电 |
| P04 | `BOARD_XL9555_04_SD_MODE` | `SD_MODE` | 输出 | 原理图 Page 2/3 | 实际连到 MAX98357A `SD_MODE`，与 TF/SD 卡无关 |
| P05 | `BOARD_XL9555_05_ESP_C5_RST` | `ESP_C5_RST` | 输出 | 原理图 Page 3 | 只引到扩展连接器 J5 |
| P06 | `BOARD_XL9555_06_MOD_SEL` | `MOD_SEL` | 输出 | 原理图 Page 3 | 只引到扩展连接器 J5 |
| P07 | `BOARD_XL9555_07_EX_PWR_EN` | `EX_PWR_EN` | 输出 | 原理图 Page 3 | 只引到扩展连接器 J5 |
| P10 | `BOARD_XL9555_10` | NC | - | 原理图 Page 3 | 当前未使用 |
| P11 | `BOARD_XL9555_11` | NC | - | 原理图 Page 3 | 当前未使用 |
| P12 | `BOARD_XL9555_12` | NC | - | 原理图 Page 3 | 当前未使用 |
| P13 | `BOARD_XL9555_13` | NC | - | 原理图 Page 3 | 当前未使用 |
| P14 | `BOARD_XL9555_14` | NC | - | 原理图 Page 3 | 当前未使用 |
| P15 | `BOARD_XL9555_15` | NC | - | 原理图 Page 3 | 当前未使用 |
| P16 | `BOARD_XL9555_16` | NC | - | 原理图 Page 3 | 当前未使用 |
| P17 | `BOARD_XL9555_17` | NC | - | 原理图 Page 3 | 当前未使用 |

## 4. QWIIC 与调试接口

| 接口 | 建议名 | 引脚/网络 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| P1 串口调试口 | `BOARD_UART0_TXD` / `BOARD_UART0_RXD` | `U0TXD` / `U0RXD` | 原理图 Page 2 | 6 Pin 接口，同时带 `VDD3V3`、`GND` |
| P6 QWIIC 接口 | `BOARD_I2C_SDA` / `BOARD_I2C_SCL` | `II2C_SDA` / `II2C_SCL` | 原理图 Page 2 | 6 Pin 接口，同时带 `VDD3V3`、`GND` |

## 5. TF 卡

| 功能 | 建议名 | GPIO | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| SD CS | `BOARD_SD_CS` | GPIO13 | 原理图 Page 2 | 共用主 SPI |
| SD SCK | `BOARD_SD_SCK` | GPIO11 | 原理图 Page 2 | 共用主 SPI |
| SD MOSI | `BOARD_SD_MOSI` | GPIO9 | 原理图 Page 2 | 共用主 SPI |
| SD MISO | `BOARD_SD_MISO` | GPIO10 | 原理图 Page 2 | 共用主 SPI |

## 6. 音频功放 MAX98357A

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| I2S BCLK | `BOARD_I2S_BCLK` | GPIO46 | 原理图 Page 2 | 原理图网名 `I2S_BCLK` |
| I2S LRCLK | `BOARD_I2S_LRCLK` | GPIO40 | 原理图 Page 2 | 原理图网名 `I2S_LRCLK` |
| I2S DIN | `BOARD_I2S_DIN` | GPIO7 | 原理图 Page 2 | ESP32-S3 -> MAX98357A |
| `SD_MODE` 控制 | `BOARD_AUDIO_SD_MODE` | `XL9555 P04` | 原理图 Page 2/3 | 功放开关/模式控制；原理图网名虽然叫 `SD_MODE`，但不是 TF 卡信号 |
| 扬声器输出 | `BOARD_SPEAKER_OUT` | `OUTP+` / `OUTN-` -> `J4` | 原理图 Page 2 | 2 Pin 扬声器接口 |

## 7. NFC ST25R3916

说明：这部分原理图标题是 `NFC`，芯片型号是 `ST25R3916`，并不是 `PN532`。

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| SPI SCK | `BOARD_NFC_SCK` | GPIO11 | 原理图 Page 2/3 | 共用主 SPI |
| SPI MOSI | `BOARD_NFC_MOSI` | GPIO9 | 原理图 Page 2/3 | 共用主 SPI |
| SPI MISO | `BOARD_NFC_MISO` | GPIO10 | 原理图 Page 2/3 | 共用主 SPI |
| CS | `BOARD_NFC_CS` | GPIO45 | 原理图 Page 2/3 | 原理图网名 `RF_CS` |
| IRQ | `BOARD_NFC_IRQ` | GPIO17 | 原理图 Page 2/3 | 原理图网名 `RF_IRQ` |
| 电源域 | `BOARD_NFC_VDD` | `LOW_PWR_3V3` | 原理图 Page 3 | 芯片多个电源脚都挂在低功耗 3.3 V 域 |

## 8. CC1101

说明：这部分原理图标题是 `CC1101`，但连到主控的网络名仍沿用了 `LORA_IO*` / `HPD_CS` 一类旧命名。

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| SPI SCK | `BOARD_CC1101_SCK` | GPIO11 | 原理图 Page 2/3 | 共用主 SPI |
| SPI MOSI | `BOARD_CC1101_MOSI` | GPIO9 | 原理图 Page 2/3 | 共用主 SPI |
| SPI MISO | `BOARD_CC1101_MISO` | GPIO10 | 原理图 Page 2/3 | 共用主 SPI |
| CS | `BOARD_CC1101_CS` | GPIO12 | 原理图 Page 2/3 | 原理图网名 `HPD_CS` |
| GDO2 | `BOARD_CC1101_GDO2` | GPIO38 | 原理图 Page 2/3 | 原理图网名 `LORA_IO2` |
| GDO0 | `BOARD_CC1101_GDO0` | GPIO8 | 原理图 Page 2/3 | 原理图网名 `LORA_IO0` |
| 开关控制 1 | `BOARD_CC1101_SW1` | `XL9555 P00` | 原理图 Page 3 | 原理图网名 `CC_SW1` |
| 开关控制 0 | `BOARD_CC1101_SW0` | `XL9555 P01` | 原理图 Page 3 | 原理图网名 `CC_SW0` |
| 电源域 | `BOARD_CC1101_VDD` | `LOW_PWR_3V3` | 原理图 Page 3 | 由低功耗 3.3 V 域供电 |

## 9. LCD

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| LCD CS | `BOARD_LCD_CS` | GPIO41 | 原理图 Page 2/4 | 原理图网名 `LCD_CS` |
| LCD DC | `BOARD_LCD_DC` | GPIO16 | 原理图 Page 2/4 | 原理图网名 `LCD_DC` |
| LCD RST | `BOARD_LCD_RST` | `XL9555 P02` | 原理图 Page 3/4 | 原理图网名 `LCD_RST`，带 RC 上电复位网络 |
| 背光使能 | `BOARD_LCD_BL_EN` | GPIO21 | 原理图 Page 2/4 | 控制 AW9364 背光驱动器 `EN` |
| SPI SCK | `BOARD_LCD_SCK` | GPIO11 | 原理图 Page 4 | 共用主 SPI |
| SPI MOSI | `BOARD_LCD_MOSI` | GPIO9 | 原理图 Page 4 | 共用主 SPI |
| SPI MISO | `BOARD_LCD_MISO` | GPIO10 | 原理图 Page 4 | 原理图标题注明已引出到 LCD FFC |
| 电源 | `BOARD_LCD_VDD` | `LCD_VDD` | 原理图 Page 4 | 由 `VDD3V3` 经磁珠 `FB1` 供电 |

## 10. RGB 灯

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| RGB 数据 | `BOARD_RGB_DATA` | GPIO14 | 原理图 Page 2/4 | 原理图网名 `RGB_DI`，驱动一串 WS2812C |
| 电源域 | `BOARD_RGB_VDD` | `LOW_PWR_3V3` | 原理图 Page 4 | 与其他低功耗外设共用 |

## 11. 红外

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| IR 发射 PWM | `BOARD_IR_TX` | GPIO15 | 原理图 Page 2/4 | 原理图网名 `IR_PWM`，经三极管驱动红外发射管 |
| IR 接收 | `BOARD_IR_RX` | GPIO1 | 原理图 Page 2/4 | 原理图网名 `IR_RX`，TSOP75338TR 输出 |
| 电源域 | `BOARD_IR_VDD` | `LOW_PWR_3V3` | 原理图 Page 4 | 接收头与发射部分都在低功耗 3.3 V 域 |

## 12. 麦克风

| 功能 | 建议名 | GPIO/映射 | 来源 | 备注 |
| --- | --- | --- | --- | --- |
| MIC DATA | `BOARD_MIC_DATA` | GPIO42 | 原理图 Page 2/4 | 原理图网名 `MIC_DATA` |
| MIC CLK | `BOARD_MIC_CLK` | GPIO39 | 原理图 Page 2/4 | 原理图网名 `MIC_CLK` |
| 电源域 | `BOARD_MIC_VDD` | `LOW_PWR_3V3` | 原理图 Page 4 | 麦克风型号 `SPM1423HM4H-B` |

## 13. 扩展连接器 J5

| J5 Pin | 信号 | 来源 | 备注 |
| --- | --- | --- | --- |
| 1 / 3 / 5 / 7 | `VSYS` | 原理图 Page 3 | 主电源轨 |
| 2 | `SPI_SCK` | 原理图 Page 3 | 主 SPI |
| 4 | `SPI_MISO` | 原理图 Page 3 | 主 SPI |
| 6 | `SPI_MOSI` | 原理图 Page 3 | 主 SPI |
| 8 | `IO48` | 原理图 Page 3 | 直接引出 |
| 10 | `IO47` | 原理图 Page 3 | 直接引出 |
| 11 | `IO0` | 原理图 Page 3 | 与 BOOT 共用 |
| 12 | `IO18` | 原理图 Page 3 | 直接引出 |
| 13 | `RST/EN` | 原理图 Page 3 | 主控复位 |
| 15 | `MOD_SEL` | 原理图 Page 3 | 由 `XL9555 P06` 驱动 |
| 17 | `EX_PWR_EN` | 原理图 Page 3 | 由 `XL9555 P07` 驱动 |
| 19 | `ESP_C5_RST` | 原理图 Page 3 | 由 `XL9555 P05` 驱动 |
| 21 | `PWR_KEY` | 原理图 Page 1/3 | 硬件电源键网络，不是普通 GPIO |
| 18 / 20 / 22 / 24 | GND | 原理图 Page 3 | 接地 |
| 9 / 14 / 16 / 23 | NC | 原理图 Page 3 | 当前未接 |

## 14. I2C 地址表

| 设备 | 地址 | 来源 | 备注 |
| --- | --- | --- | --- |
| XL9555 | `0x22` | 原理图 Page 3 | IO 扩展 |
| BQ27220 | `0x55` | 原理图 Page 1 | 电量计 |
| BQ25896 | `0x6B` | 原理图 Page 1 | 充电管理，丝印同时兼容 SY6970 |
| SY6970 | `0x6A` | 原理图 Page 1 | 充电管理，和 BQ25896 位置复用 |

## 15. 电源域与命名提示

- `LOW_PWR_EN` 由 `XL9555 P03` 控制，打开后给 `LOW_PWR_3V3` 上电；NFC、CC1101、RGB、IR、MIC，以及功放电源 `SPK_VDD` 都依赖这个电源域。
- PDF 文件名叫 `T-Embed-PN532`，但 NFC 主芯片实际是 `ST25R3916`；写驱动或宏名时建议按真实器件命名。
- `LORA_IO0`、`LORA_IO2`、`HPD_CS` 这几个网名在这块板上实际服务于 `CC1101`，不建议继续沿用成对外 API 名称。
- `SD_MODE` 在这块板上是 `MAX98357A` 的开关/模式脚，不是 TF 卡模式脚。
- `TXD0` / `RXD0` 在原理图中没有直接给出数值 GPIO；如果后续代码要写死 GPIO 号，建议再对照一次 ESP32-S3 模组资料。
- 当前仓库里还没有现成的板级公共头文件，后续如果开始写固件，建议把本文的建议宏名统一收敛到单一 `board_config.h`。
