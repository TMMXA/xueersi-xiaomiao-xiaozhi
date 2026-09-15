#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "wifi_board.h"

#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7735.h>
#include <esp_log.h>

#define TAG "XueersiXiaomiao"

static_assert(AUDIO_INPUT_SAMPLE_RATE == AUDIO_OUTPUT_SAMPLE_RATE,
              "Xiaomiao RX and TX share BCLK/WS and must use one sample rate");
static_assert(AUDIO_INPUT_SAMPLE_RATE == 24000,
              "The current Xiaomiao hardware profile is intentionally 24 kHz");

namespace {

constexpr uint8_t kFrameRate[] = {0x01, 0x2C, 0x2D};
constexpr uint8_t kPartialFrameRate[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
constexpr uint8_t kInversionControl[] = {0x07};
constexpr uint8_t kPowerControl1[] = {0xA2, 0x02, 0x84};
constexpr uint8_t kPowerControl2[] = {0xC5};
constexpr uint8_t kPowerControl3[] = {0x0A, 0x00};
constexpr uint8_t kPowerControl4[] = {0x8A, 0x2A};
constexpr uint8_t kPowerControl5[] = {0x8A, 0xEE};
constexpr uint8_t kVcomControl[] = {0x0E};
constexpr uint8_t kMadctlPortrait[] = {0xC0};
constexpr uint8_t kColorMode[] = {0x05};
constexpr uint8_t kColumnRange[] = {0x00, 0x00, 0x00, 0x7F};
constexpr uint8_t kRowRange[] = {0x00, 0x00, 0x00, 0x9F};
constexpr uint8_t kPositiveGamma[] = {
    0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
    0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
};
constexpr uint8_t kNegativeGamma[] = {
    0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
};

// This is the black-tab initialization used by working Xiaomiao firmware. The
// panel driver applies the final 90-degree rotation after this table runs.
const st7735_lcd_init_cmd_t kLcdInitCommands[] = {
    {ST7735_DISPOFF, nullptr, 0, 0},
    {ST7735_SWRESET, nullptr, 0, 150},
    {ST7735_SLPOUT, nullptr, 0, 500},
    {ST7735_FRMCTR1, kFrameRate, sizeof(kFrameRate), 0},
    {ST7735_FRMCTR2, kFrameRate, sizeof(kFrameRate), 0},
    {ST7735_FRMCTR3, kPartialFrameRate, sizeof(kPartialFrameRate), 0},
    {ST7735_INVCTR, kInversionControl, sizeof(kInversionControl), 0},
    {ST7735_PWCTR1, kPowerControl1, sizeof(kPowerControl1), 0},
    {ST7735_PWCTR2, kPowerControl2, sizeof(kPowerControl2), 0},
    {ST7735_PWCTR3, kPowerControl3, sizeof(kPowerControl3), 0},
    {ST7735_PWCTR4, kPowerControl4, sizeof(kPowerControl4), 0},
    {ST7735_PWCTR5, kPowerControl5, sizeof(kPowerControl5), 0},
    {ST7735_VMCTR1, kVcomControl, sizeof(kVcomControl), 0},
    {ST7735_INVOFF, nullptr, 0, 0},
    {ST7735_MADCTL, kMadctlPortrait, sizeof(kMadctlPortrait), 0},
    {ST7735_COLMOD, kColorMode, sizeof(kColorMode), 0},
    {ST7735_CASET, kColumnRange, sizeof(kColumnRange), 0},
    {ST7735_RASET, kRowRange, sizeof(kRowRange), 0},
    {ST7735_GMCTRP1, kPositiveGamma, sizeof(kPositiveGamma), 0},
    {ST7735_GMCTRN1, kNegativeGamma, sizeof(kNegativeGamma), 0},
    {ST7735_NORON, nullptr, 0, 10},
};

button_handle_t CreateButton(gpio_num_t gpio_num, bool disable_pull) {
    button_config_t button_config = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_config = {
        .gpio_num = gpio_num,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = disable_pull,
    };
    button_handle_t handle = nullptr;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_config, &handle));
    return handle;
}

}  // namespace

class XueersiXiaomiao : public WifiBoard {
private:
    Button up_button_;
    Button down_button_;
    Button left_button_;
    Button right_button_;
    Button a_button_;
    Button b_button_;
    LcdDisplay* display_ = nullptr;

    void InitializeSpi() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DISPLAY_MOSI_PIN;
        bus_config.miso_io_num = DISPLAY_MISO_PIN;
        bus_config.sclk_io_num = DISPLAY_CLK_PIN;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = DISPLAY_NATIVE_WIDTH * DISPLAY_NATIVE_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_PIXEL_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        const st7735_vendor_config_t vendor_config = {
            .init_cmds = kLcdInitCommands,
            .init_cmds_size = sizeof(kLcdInitCommands) / sizeof(kLcdInitCommands[0]),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = const_cast<st7735_vendor_config_t*>(&vendor_config);
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void SetVolume(int delta) {
        if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
            return;
        }
        auto* codec = GetAudioCodec();
        int volume = codec->output_volume() + delta;
        if (volume < 0) {
            volume = 0;
        } else if (volume > 100) {
            volume = 100;
        }
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void SetTheme(const char* name) {
        // LcdDisplay::SetTheme expects SetupUI() to have created all labels.
        if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
            return;
        }
        auto* theme = LvglThemeManager::GetInstance().GetTheme(name);
        if (theme != nullptr) {
            GetDisplay()->SetTheme(theme);
        }
    }

    void InitializeButtons() {
        a_button_.OnPressDown([]() {
            ESP_LOGI(TAG, "A/PTT pressed: start listening");
            Application::GetInstance().Schedule([]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() != kDeviceStateStarting) {
                    app.StartListening();
                }
            });
        });
        a_button_.OnPressUp([]() {
            ESP_LOGI(TAG, "A/PTT released: stop listening");
            Application::GetInstance().Schedule([]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() != kDeviceStateStarting) {
                    app.StopListening();
                }
            });
        });

        b_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    EnterWifiConfigMode();
                } else {
                    app.ToggleChatState();
                }
            });
        });
        b_button_.OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() { EnterWifiConfigMode(); });
        });

        up_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() { SetVolume(10); });
        });
        up_button_.OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
                    return;
                }
                GetAudioCodec()->SetOutputVolume(100);
                GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
            });
        });
        down_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() { SetVolume(-10); });
        });
        down_button_.OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
                    return;
                }
                GetAudioCodec()->SetOutputVolume(0);
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            });
        });

        left_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() { SetTheme("light"); });
        });
        right_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() { SetTheme("dark"); });
        });
    }

public:
    XueersiXiaomiao()
        : up_button_(CreateButton(UP_BUTTON_GPIO, false)),
          down_button_(CreateButton(DOWN_BUTTON_GPIO, false)),
          left_button_(CreateButton(LEFT_BUTTON_GPIO, false)),
          right_button_(CreateButton(RIGHT_BUTTON_GPIO, true)),
          a_button_(CreateButton(A_BUTTON_GPIO, true)),
          b_button_(CreateButton(B_BUTTON_GPIO, false)) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                              AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(XueersiXiaomiao);
