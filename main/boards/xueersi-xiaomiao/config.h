#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// The microphone and speaker share one I2S clock bus on the modified console.
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_26
#define AUDIO_I2S_GPIO_WS GPIO_NUM_25
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_32
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_33

// The on-board A button is active-low and has an external pull-up.
#define PTT_BUTTON_GPIO GPIO_NUM_34

#endif  // _BOARD_CONFIG_H_
