#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// AtomMatrix+EchoBase Board configuration

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

/*
引脚：
PORT_A_G25_黄 : SDA
PORT_A_G32_白 : SCL

PORT_B_GND_黑 : MLCK    <-> MCLK
PORT_B_G26_黄 : SCLK    <-> BCLK
PORT_B_G33_白 : DIN     <-> DOUT

PORT_C_G18_黄 : WS      <-> LRCK
PORT_C_G19_白 : DOUT    <-> DIN
PORT_B_5V_红  : 5V
PORT_B_GND_黑 : GND
*/
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_26
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_33
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_18
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_19

#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_25
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_32
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_GPIO_PA     GPIO_NUM_NC

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_38

#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_37
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

#define EPD_MAIN_PWR_PIN        GPIO_NUM_2
#define EXT_PWR_EN_GPIO         GPIO_NUM_5

#define EXT_I2C_NUM             I2C_NUM_0

// M5Paper IT8951 EPD Display Configuration
// SPI for IT8951 (shared with TF card)
#define EPD_SPI_NUM SPI2_HOST

// IT8951 SPI pins (per M5Paper schematic)
// GPIO 12: MOSI (shared with TF card)
// GPIO 13: MISO (shared with TF card)
// GPIO 14: SCK (shared with TF card)
// GPIO 15: CS for IT8951
// GPIO 23: RST for IT8951
// GPIO 27: BUSY for IT8951
#define EPD_GPIO_MOSI GPIO_NUM_12
#define EPD_GPIO_MISO GPIO_NUM_13
#define EPD_GPIO_SCK GPIO_NUM_14
#define EPD_GPIO_CS GPIO_NUM_15
#define EPD_GPIO_RST GPIO_NUM_23
#define EPD_GPIO_BUSY GPIO_NUM_27

// Display dimensions
#define EPD_WIDTH 960
#define EPD_HEIGHT 540
#define EPD_GRAYS 16

// Refresh intervals
#define EPD_FULL_REFRESH_INTERVAL_MS 300000  // 5 minutes for full refresh
#define EPD_PARTIAL_REFRESH_INTERVAL_MS 1000 // 1 second minimum between partial

#endif // _BOARD_CONFIG_H_