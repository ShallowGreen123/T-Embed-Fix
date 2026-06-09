# T-Embed-Fix



~~~c

// IIC addr
#define BOARD_I2C_XL9555  0x22  // XL9555
#define BOARD_I2C_BQ27220 0x55  // BQ27220
// #define BOARD_I2C_BQ25896 0x6b  // BQ25896  // Deprecated
#define BOARD_I2C_SY6970  0x6a  // SY6970

// --------- IIC ---------
#define BOARD_I2C_SDA  3
#define BOARD_I2C_SCL  2

// --------- SPI ---------
#define BOARD_SPI_SCK  11
#define BOARD_SPI_MOSI 9
#define BOARD_SPI_MISO 10

// XL9555
#define BOARD_XL9555_CC_SW1             0
#define BOARD_XL9555_CC_SW0             1
#define BOARD_XL9555_LCD_RST            2
#define BOARD_XL9555_LOW_PWR_EN         3
#define BOARD_XL9555_AP_EN              4     // Amplifier enable
#define BOARD_XL9555_ESP32C5_RST        5
#define BOARD_XL9555_MOD_SEL            6
#define BOARD_XL9555_EX_PWR_EN          7
#define BOARD_XL9555_10                 8
#define BOARD_XL9555_11                 9
#define BOARD_XL9555_12                 10
#define BOARD_XL9555_13                 11
#define BOARD_XL9555_14                 12
#define BOARD_XL9555_15                 13
#define BOARD_XL9555_16                 14
#define BOARD_XL9555_17                 15

// --------- ENCODER ---------
#define ENCODER_INA 4
#define ENCODER_INB 5
#define ENCODER_KEY 0

// KEY
#define BOARD_USER_KEY 6

// TF card
#define BOARD_SD_CS   13
#define BOARD_SD_SCK  BOARD_SPI_SCK
#define BOARD_SD_MOSI BOARD_SPI_MOSI
#define BOARD_SD_MISO BOARD_SPI_MISO

// MAX98357A
#define BOARD_VOICE_BCLK  46
#define BOARD_VOICE_LRCLK 40
#define BOARD_VOICE_DIN   7
#define BOARD_VOICE_AP_EN BOARD_XL9555_AP_EN // Amplifier enable pin

// NFC ST25R3916
#define BOARD_NFC_SCK   BOARD_SPI_SCK
#define BOARD_NFC_MOSI  BOARD_SPI_MOSI
#define BOARD_NFC_MISO  BOARD_SPI_MISO
#define BOARD_NFC_CS    45
#define BOARD_NFC_IRQ   17

// CC1101
#define BOARD_CC1101_SCK    BOARD_SPI_SCK
#define BOARD_CC1101_MOSI   BOARD_SPI_MOSI
#define BOARD_CC1101_MISO   BOARD_SPI_MISO
#define BOARD_CC1101_CS     12
#define BOARD_CC1101_GDO0   8
#define BOARD_CC1101_GDO2   38
#define BOARD_CC1101_SW1    BOARD_XL9555_CC_SW1
#define BOARD_CC1101_SW0    BOARD_XL9555_CC_SW0

// LCD
#define BOARD_LCD_WIDTH  170
#define BOARD_LCD_HEIGHT 320
#define BOARD_LCD_SCK   BOARD_SPI_SCK
#define BOARD_LCD_MOSI  BOARD_SPI_MOSI
#define BOARD_LCD_MISO  BOARD_SPI_MISO
#define BOARD_LCD_CS    41
#define BOARD_LCD_DC    16
#define BOARD_LCD_RST   BOARD_XL9555_LCD_RST
#define BOARD_LCD_BL    21

// WS2812
#define BOARD_WS2812_NUM_LEDS 8
#define BOARD_WS2812_DATA_PIN 14

// IR
#define BOARD_IR_TX 15
#define BOARD_IR_RX 1

// MIC
#define BOARD_MIC_DATA 42
#define BOARD_MIC_CLK  39
~~~


