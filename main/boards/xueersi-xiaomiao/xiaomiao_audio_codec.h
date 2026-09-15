#ifndef _XIAOMIAO_AUDIO_CODEC_H_
#define _XIAOMIAO_AUDIO_CODEC_H_

#include "codecs/no_audio_codec.h"

#include <vector>

// The Xiaomiao audio add-on shares BCLK/WS between an ICS43434 microphone and
// a MAX98357A amplifier. Both devices require a conventional two-slot I2S
// frame even though the application audio is mono.
class XiaomiaoAudioCodec : public NoAudioCodec {
public:
    XiaomiaoAudioCodec(int sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,
                       gpio_num_t din);

protected:
    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;
    void EnableInput(bool enable) override;

private:
    std::vector<int32_t> rx_buffer_;
    std::vector<int32_t> tx_buffer_;
    int64_t last_level_log_time_ = 0;
    int64_t last_error_log_time_ = 0;
    bool read_entered_logged_ = false;
};

#endif  // _XIAOMIAO_AUDIO_CODEC_H_
