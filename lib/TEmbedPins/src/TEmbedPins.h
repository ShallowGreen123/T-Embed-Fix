#pragma once

#include <Arduino.h>

namespace t_embed {

namespace i2c_address {
constexpr uint8_t kXl9555 = 0x22;
constexpr uint8_t kBq27220 = 0x55;
constexpr uint8_t kSy6970 = 0x6A;
}  // namespace i2c_address

namespace xl9555 {
constexpr uint8_t kCcSw1 = 0;
constexpr uint8_t kCcSw0 = 1;
constexpr uint8_t kLcdRst = 2;
constexpr uint8_t kLowPowerEnable = 3;
constexpr uint8_t kAmplifierEnable = 4;
constexpr uint8_t kEsp32C5Rst = 5;
constexpr uint8_t kModSel = 6;
constexpr uint8_t kExPowerEnable = 7;
constexpr uint8_t kIo10 = 8;
constexpr uint8_t kIo11 = 9;
constexpr uint8_t kIo12 = 10;
constexpr uint8_t kIo13 = 11;
constexpr uint8_t kIo14 = 12;
constexpr uint8_t kIo15 = 13;
constexpr uint8_t kIo16 = 14;
constexpr uint8_t kIo17 = 15;
}  // namespace xl9555

namespace pin {
constexpr uint8_t kI2cSda = 3;
constexpr uint8_t kI2cScl = 2;

constexpr uint8_t kSpiSck = 11;
constexpr uint8_t kSpiMosi = 9;
constexpr uint8_t kSpiMiso = 10;

constexpr uint8_t kEncoderIna = 4;
constexpr uint8_t kEncoderInb = 5;
constexpr uint8_t kEncoderKey = 0;
constexpr uint8_t kUserKey = 6;

constexpr uint8_t kSdCs = 13;

constexpr uint8_t kVoiceBclk = 46;
constexpr uint8_t kVoiceLrclk = 40;
constexpr uint8_t kVoiceDin = 7;

constexpr uint8_t kNfcCs = 45;
constexpr uint8_t kNfcIrq = 17;

constexpr uint8_t kCc1101Cs = 12;
constexpr uint8_t kCc1101Gdo0 = 8;
constexpr uint8_t kCc1101Gdo2 = 38;

constexpr uint8_t kLcdCs = 41;
constexpr uint8_t kLcdDc = 16;
constexpr uint8_t kLcdBl = 21;
constexpr uint16_t kLcdWidth = 170;
constexpr uint16_t kLcdHeight = 320;

constexpr uint8_t kWs2812Data = 14;
constexpr uint8_t kWs2812Count = 8;

constexpr uint8_t kIrTx = 15;
constexpr uint8_t kIrRx = 1;

constexpr uint8_t kMicData = 42;
constexpr uint8_t kMicClk = 39;
}  // namespace pin

}  // namespace t_embed

// README compatibility aliases
#ifndef BOARD_I2C_XL9555
#define BOARD_I2C_XL9555 (::t_embed::i2c_address::kXl9555)
#endif

#ifndef BOARD_I2C_BQ27220
#define BOARD_I2C_BQ27220 (::t_embed::i2c_address::kBq27220)
#endif

#ifndef BOARD_I2C_SY6970
#define BOARD_I2C_SY6970 (::t_embed::i2c_address::kSy6970)
#endif

#ifndef BOARD_I2C_SDA
#define BOARD_I2C_SDA (::t_embed::pin::kI2cSda)
#endif

#ifndef BOARD_I2C_SCL
#define BOARD_I2C_SCL (::t_embed::pin::kI2cScl)
#endif

#ifndef BOARD_SPI_SCK
#define BOARD_SPI_SCK (::t_embed::pin::kSpiSck)
#endif

#ifndef BOARD_SPI_MOSI
#define BOARD_SPI_MOSI (::t_embed::pin::kSpiMosi)
#endif

#ifndef BOARD_SPI_MISO
#define BOARD_SPI_MISO (::t_embed::pin::kSpiMiso)
#endif

#ifndef BOARD_XL9555_CC_SW1
#define BOARD_XL9555_CC_SW1 (::t_embed::xl9555::kCcSw1)
#endif

#ifndef BOARD_XL9555_CC_SW0
#define BOARD_XL9555_CC_SW0 (::t_embed::xl9555::kCcSw0)
#endif

#ifndef BOARD_XL9555_LCD_RST
#define BOARD_XL9555_LCD_RST (::t_embed::xl9555::kLcdRst)
#endif

#ifndef BOARD_XL9555_LOW_PWR_EN
#define BOARD_XL9555_LOW_PWR_EN (::t_embed::xl9555::kLowPowerEnable)
#endif

#ifndef BOARD_XL9555_AP_EN
#define BOARD_XL9555_AP_EN (::t_embed::xl9555::kAmplifierEnable)
#endif

#ifndef BOARD_XL9555_ESP32C5_RST
#define BOARD_XL9555_ESP32C5_RST (::t_embed::xl9555::kEsp32C5Rst)
#endif

#ifndef BOARD_XL9555_MOD_SEL
#define BOARD_XL9555_MOD_SEL (::t_embed::xl9555::kModSel)
#endif

#ifndef BOARD_XL9555_EX_PWR_EN
#define BOARD_XL9555_EX_PWR_EN (::t_embed::xl9555::kExPowerEnable)
#endif

#ifndef BOARD_XL9555_10
#define BOARD_XL9555_10 (::t_embed::xl9555::kIo10)
#endif

#ifndef BOARD_XL9555_11
#define BOARD_XL9555_11 (::t_embed::xl9555::kIo11)
#endif

#ifndef BOARD_XL9555_12
#define BOARD_XL9555_12 (::t_embed::xl9555::kIo12)
#endif

#ifndef BOARD_XL9555_13
#define BOARD_XL9555_13 (::t_embed::xl9555::kIo13)
#endif

#ifndef BOARD_XL9555_14
#define BOARD_XL9555_14 (::t_embed::xl9555::kIo14)
#endif

#ifndef BOARD_XL9555_15
#define BOARD_XL9555_15 (::t_embed::xl9555::kIo15)
#endif

#ifndef BOARD_XL9555_16
#define BOARD_XL9555_16 (::t_embed::xl9555::kIo16)
#endif

#ifndef BOARD_XL9555_17
#define BOARD_XL9555_17 (::t_embed::xl9555::kIo17)
#endif

#ifndef ENCODER_INA
#define ENCODER_INA (::t_embed::pin::kEncoderIna)
#endif

#ifndef ENCODER_INB
#define ENCODER_INB (::t_embed::pin::kEncoderInb)
#endif

#ifndef ENCODER_KEY
#define ENCODER_KEY (::t_embed::pin::kEncoderKey)
#endif

#ifndef BOARD_USER_KEY
#define BOARD_USER_KEY (::t_embed::pin::kUserKey)
#endif

#ifndef BOARD_SD_CS
#define BOARD_SD_CS (::t_embed::pin::kSdCs)
#endif

#ifndef BOARD_SD_SCK
#define BOARD_SD_SCK BOARD_SPI_SCK
#endif

#ifndef BOARD_SD_MOSI
#define BOARD_SD_MOSI BOARD_SPI_MOSI
#endif

#ifndef BOARD_SD_MISO
#define BOARD_SD_MISO BOARD_SPI_MISO
#endif

#ifndef BOARD_VOICE_BCLK
#define BOARD_VOICE_BCLK (::t_embed::pin::kVoiceBclk)
#endif

#ifndef BOARD_VOICE_LRCLK
#define BOARD_VOICE_LRCLK (::t_embed::pin::kVoiceLrclk)
#endif

#ifndef BOARD_VOICE_DIN
#define BOARD_VOICE_DIN (::t_embed::pin::kVoiceDin)
#endif

#ifndef BOARD_VOICE_AP_EN
#define BOARD_VOICE_AP_EN BOARD_XL9555_AP_EN
#endif

#ifndef BOARD_NFC_SCK
#define BOARD_NFC_SCK BOARD_SPI_SCK
#endif

#ifndef BOARD_NFC_MOSI
#define BOARD_NFC_MOSI BOARD_SPI_MOSI
#endif

#ifndef BOARD_NFC_MISO
#define BOARD_NFC_MISO BOARD_SPI_MISO
#endif

#ifndef BOARD_NFC_CS
#define BOARD_NFC_CS (::t_embed::pin::kNfcCs)
#endif

#ifndef BOARD_NFC_IRQ
#define BOARD_NFC_IRQ (::t_embed::pin::kNfcIrq)
#endif

#ifndef BOARD_CC1101_SCK
#define BOARD_CC1101_SCK BOARD_SPI_SCK
#endif

#ifndef BOARD_CC1101_MOSI
#define BOARD_CC1101_MOSI BOARD_SPI_MOSI
#endif

#ifndef BOARD_CC1101_MISO
#define BOARD_CC1101_MISO BOARD_SPI_MISO
#endif

#ifndef BOARD_CC1101_CS
#define BOARD_CC1101_CS (::t_embed::pin::kCc1101Cs)
#endif

#ifndef BOARD_CC1101_GDO0
#define BOARD_CC1101_GDO0 (::t_embed::pin::kCc1101Gdo0)
#endif

#ifndef BOARD_CC1101_GDO2
#define BOARD_CC1101_GDO2 (::t_embed::pin::kCc1101Gdo2)
#endif

#ifndef BOARD_CC1101_SW1
#define BOARD_CC1101_SW1 BOARD_XL9555_CC_SW1
#endif

#ifndef BOARD_CC1101_SW0
#define BOARD_CC1101_SW0 BOARD_XL9555_CC_SW0
#endif

#ifndef BOARD_LCD_WIDTH
#define BOARD_LCD_WIDTH (::t_embed::pin::kLcdWidth)
#endif

#ifndef BOARD_LCD_HEIGHT
#define BOARD_LCD_HEIGHT (::t_embed::pin::kLcdHeight)
#endif

#ifndef BOARD_LCD_SCK
#define BOARD_LCD_SCK BOARD_SPI_SCK
#endif

#ifndef BOARD_LCD_MOSI
#define BOARD_LCD_MOSI BOARD_SPI_MOSI
#endif

#ifndef BOARD_LCD_MISO
#define BOARD_LCD_MISO BOARD_SPI_MISO
#endif

#ifndef BOARD_LCD_CS
#define BOARD_LCD_CS (::t_embed::pin::kLcdCs)
#endif

#ifndef BOARD_LCD_DC
#define BOARD_LCD_DC (::t_embed::pin::kLcdDc)
#endif

#ifndef BOARD_LCD_RST
#define BOARD_LCD_RST BOARD_XL9555_LCD_RST
#endif

#ifndef BOARD_LCD_BL
#define BOARD_LCD_BL (::t_embed::pin::kLcdBl)
#endif

#ifndef BOARD_WS2812_NUM_LEDS
#define BOARD_WS2812_NUM_LEDS (::t_embed::pin::kWs2812Count)
#endif

#ifndef BOARD_WS2812_DATA_PIN
#define BOARD_WS2812_DATA_PIN (::t_embed::pin::kWs2812Data)
#endif

#ifndef BOARD_IR_TX
#define BOARD_IR_TX (::t_embed::pin::kIrTx)
#endif

#ifndef BOARD_IR_RX
#define BOARD_IR_RX (::t_embed::pin::kIrRx)
#endif

#ifndef BOARD_MIC_DATA
#define BOARD_MIC_DATA (::t_embed::pin::kMicData)
#endif

#ifndef BOARD_MIC_CLK
#define BOARD_MIC_CLK (::t_embed::pin::kMicClk)
#endif
