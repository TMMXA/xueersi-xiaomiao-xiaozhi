#include "xiaomiao_audio_codec.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#define TAG "XiaomiaoAudio"

XiaomiaoAudioCodec::XiaomiaoAudioCodec(int sample_rate, gpio_num_t bclk, gpio_num_t ws,
                                       gpio_num_t dout, gpio_num_t din) {
    duplex_ = true;
    input_sample_rate_ = sample_rate;
    output_sample_rate_ = sample_rate;
    input_channels_ = 1;
    output_channels_ = 1;

    i2s_chan_config_t channel_config = {
        .id = XIAOZHI_I2S_PORT(0),
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &tx_handle_, &rx_handle_));

    i2s_std_config_t standard_config = {
        .clk_cfg =
            {
                .sample_rate_hz = static_cast<uint32_t>(sample_rate),
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                .ext_clk_freq_hz = 0,
#endif
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = 32,
                .ws_pol = false,
                .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
#endif
            },
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = din,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &standard_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &standard_config));
    ESP_LOGI(TAG, "Stereo-framed duplex I2S created at %d Hz", sample_rate);
}

int XiaomiaoAudioCodec::Read(int16_t* dest, int samples) {
    constexpr uint32_t kReadTimeoutMs = 200;
    constexpr int kPhysicalChannels = 2;

    if (!read_entered_logged_) {
        read_entered_logged_ = true;
        ESP_LOGI(TAG, "Read() entered (input task is calling the codec)");
    }

    rx_buffer_.resize(static_cast<size_t>(samples) * kPhysicalChannels);
    size_t bytes_read = 0;
    const esp_err_t err = i2s_channel_read(rx_handle_, rx_buffer_.data(),
                                           rx_buffer_.size() * sizeof(rx_buffer_[0]), &bytes_read,
                                           kReadTimeoutMs);
    if (err != ESP_OK) {
        const int64_t now = esp_timer_get_time();
        if (now - last_error_log_time_ >= 1000000) {
            last_error_log_time_ = now;
            ESP_LOGW(TAG, "i2s_channel_read failed: %s", esp_err_to_name(err));
        }
        return 0;
    }

    const int frames_read = static_cast<int>(bytes_read / (sizeof(int32_t) * kPhysicalChannels));
    int peak = 0;
    int64_t square_sum = 0;
    for (int i = 0; i < frames_read; ++i) {
        // The module's ICS43434 L/R pin selects the left (WS-low) slot. Its
        // 24-bit sample is left-aligned in the 32-bit I2S word.
        const int32_t sample = rx_buffer_[static_cast<size_t>(i) * kPhysicalChannels] >> 12;
        const int16_t pcm = static_cast<int16_t>(
            std::clamp(sample, static_cast<int32_t>(INT16_MIN), static_cast<int32_t>(INT16_MAX)));
        dest[i] = pcm;
        const int magnitude = std::abs(static_cast<int>(pcm));
        peak = std::max(peak, magnitude);
        square_sum += static_cast<int64_t>(pcm) * pcm;
    }

    const int64_t now = esp_timer_get_time();
    if (frames_read > 0 && now - last_level_log_time_ >= 1000000) {
        last_level_log_time_ = now;
        const int rms = static_cast<int>(std::sqrt(static_cast<double>(square_sum) / frames_read));
        ESP_LOGI(TAG, "Microphone level: peak=%d rms=%d", peak, rms);
    }
    return frames_read;
}

int XiaomiaoAudioCodec::Write(const int16_t* data, int samples) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    constexpr int kPhysicalChannels = 2;

    tx_buffer_.resize(static_cast<size_t>(samples) * kPhysicalChannels);
    const int32_t volume_factor =
        static_cast<int32_t>(std::pow(static_cast<double>(output_volume_) / 100.0, 2) * 65536);
    for (int i = 0; i < samples; ++i) {
        const int64_t scaled = static_cast<int64_t>(data[i]) * volume_factor;
        const int32_t sample = static_cast<int32_t>(
            std::clamp(scaled, static_cast<int64_t>(INT32_MIN), static_cast<int64_t>(INT32_MAX)));
        tx_buffer_[static_cast<size_t>(i) * kPhysicalChannels] = sample;
        tx_buffer_[static_cast<size_t>(i) * kPhysicalChannels + 1] = sample;
    }

    size_t bytes_written = 0;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle_, tx_buffer_.data(),
                                      tx_buffer_.size() * sizeof(tx_buffer_[0]), &bytes_written,
                                      portMAX_DELAY));
    return static_cast<int>(bytes_written / (sizeof(int32_t) * kPhysicalChannels));
}

void XiaomiaoAudioCodec::EnableInput(bool enable) {
    if (enable) {
        // Classic ESP32 full-duplex: RX is slave and needs TX clocks first.
        if (!output_enabled_ && tx_handle_ != nullptr) {
            std::lock_guard<std::mutex> lock(data_if_mutex_);
            if (!output_enabled_) {
                ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
                AudioCodec::EnableOutput(true);
                ESP_LOGI(TAG, "Enabled TX clock source before RX (shared BCLK/WS)");
            }
        }
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (!rx_hw_enabled_) {
            ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
            rx_hw_enabled_ = true;
        }
        AudioCodec::EnableInput(true);
        return;
    }

    std::lock_guard<std::mutex> lock(data_if_mutex_);
    AudioCodec::EnableInput(false);
    // Disabling slave RX while TX is playing can stall classic-ESP32
    // full-duplex output. Keep RX until both directions are idle.
    if (!output_enabled_ && rx_hw_enabled_) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        rx_hw_enabled_ = false;
        ESP_LOGI(TAG, "Disabled RX after both directions went idle");
    } else if (rx_hw_enabled_) {
        ESP_LOGI(TAG, "Keeping RX enabled while TX is active (shared duplex)");
    }
}

void XiaomiaoAudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable != output_enabled_) {
        if (enable) {
            ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
        } else {
            ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
        }
        AudioCodec::EnableOutput(enable);
    }
    if (!enable && !input_enabled_ && rx_hw_enabled_) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        rx_hw_enabled_ = false;
        ESP_LOGI(TAG, "Disabled RX after both directions went idle");
    }
}
