#ifndef POCKET_ESP_AI_C3_CONFIG_H
#define POCKET_ESP_AI_C3_CONFIG_H

#include <driver/gpio.h>

// The INMP441 microphone and MAX98357 speaker share one full-duplex I2S clock.
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_2
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_1
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_3
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_8

// GM009605 v4.3 is a 128x64 SSD1306-compatible I2C OLED.
#define DISPLAY_SDA_PIN GPIO_NUM_20
#define DISPLAY_SCL_PIN GPIO_NUM_10
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

// GPIO9 is the ESP32-C3 BOOT strap and is released to GPIO after reset.
#define BOOT_BUTTON_GPIO GPIO_NUM_9

#endif  // POCKET_ESP_AI_C3_CONFIG_H
