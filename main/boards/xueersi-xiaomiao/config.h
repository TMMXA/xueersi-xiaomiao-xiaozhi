#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_types.h>

// The microphone and speaker share one I2S clock bus on the modified console.
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_26
#define AUDIO_I2S_GPIO_WS GPIO_NUM_25
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_32
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_33

// ST7735 TFT. The MicroSD card shares this SPI2 bus and uses GPIO22 for CS.
#define DISPLAY_SPI_HOST SPI2_HOST
#define DISPLAY_MOSI_PIN GPIO_NUM_23
#define DISPLAY_MISO_PIN GPIO_NUM_19
#define DISPLAY_CLK_PIN GPIO_NUM_18
#define DISPLAY_CS_PIN GPIO_NUM_5
#define DISPLAY_DC_PIN GPIO_NUM_4
#define DISPLAY_RST_PIN GPIO_NUM_NC
#define DISPLAY_NATIVE_WIDTH 128
#define DISPLAY_NATIVE_HEIGHT 160
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true
#define DISPLAY_INVERT_COLOR false
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_SPI_MODE 0
#define DISPLAY_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

// All six keys are active-low. GPIO34 and GPIO35 are input-only and have no
// internal pull resistors; the Xiaomiao PCB provides their idle-high bias.
#define UP_BUTTON_GPIO GPIO_NUM_2
#define DOWN_BUTTON_GPIO GPIO_NUM_13
#define LEFT_BUTTON_GPIO GPIO_NUM_27
#define RIGHT_BUTTON_GPIO GPIO_NUM_35
#define A_BUTTON_GPIO GPIO_NUM_34
#define B_BUTTON_GPIO GPIO_NUM_12

#endif  // _BOARD_CONFIG_H_
