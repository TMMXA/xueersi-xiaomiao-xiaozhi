#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "wifi_board.h"

#include <esp_log.h>

#define TAG "XueersiXiaomiao"

class XueersiXiaomiao : public WifiBoard {
private:
    Button ptt_button_;

    static button_handle_t CreatePttButton() {
        button_config_t button_config = {
            .long_press_time = 0,
            .short_press_time = 0,
        };
        button_gpio_config_t gpio_config = {
            .gpio_num = PTT_BUTTON_GPIO,
            .active_level = 0,
            .enable_power_save = false,
            // GPIO34 is input-only and has no internal pull resistor. The board
            // supplies the idle-high bias used by the original firmware.
            .disable_pull = true,
        };
        button_handle_t handle = nullptr;
        ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_config, &handle));
        return handle;
    }

    void InitializeButton() {
        ptt_button_.OnPressDown([]() {
            ESP_LOGI(TAG, "PTT pressed: start listening");
            Application::GetInstance().Schedule(
                []() { Application::GetInstance().StartListening(); });
        });
        ptt_button_.OnPressUp([]() {
            ESP_LOGI(TAG, "PTT released: stop listening");
            Application::GetInstance().Schedule(
                []() { Application::GetInstance().StopListening(); });
        });
    }

public:
    XueersiXiaomiao() : ptt_button_(CreatePttButton()) { InitializeButton(); }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                              AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }
};

DECLARE_BOARD(XueersiXiaomiao);
