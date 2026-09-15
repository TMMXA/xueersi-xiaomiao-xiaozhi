import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BOARD_DIR = ROOT / "main/boards/xueersi-xiaomiao"


def read_defines():
    config_h = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
    return dict(re.findall(r"^#define\s+(\w+)\s+(.+?)\s*$", config_h, re.MULTILINE))


class XueersiXiaomiaoBoardTests(unittest.TestCase):
    def test_audio_clock_and_data_pins(self):
        defines = read_defines()
        self.assertEqual(defines["AUDIO_INPUT_SAMPLE_RATE"], "24000")
        self.assertEqual(defines["AUDIO_OUTPUT_SAMPLE_RATE"], "24000")
        self.assertEqual(defines["AUDIO_I2S_GPIO_BCLK"], "GPIO_NUM_26")
        self.assertEqual(defines["AUDIO_I2S_GPIO_WS"], "GPIO_NUM_25")
        self.assertEqual(defines["AUDIO_I2S_GPIO_DIN"], "GPIO_NUM_32")
        self.assertEqual(defines["AUDIO_I2S_GPIO_DOUT"], "GPIO_NUM_33")

    def test_display_and_button_pinout(self):
        defines = read_defines()
        expected = {
            "DISPLAY_MOSI_PIN": "GPIO_NUM_23",
            "DISPLAY_MISO_PIN": "GPIO_NUM_19",
            "DISPLAY_CLK_PIN": "GPIO_NUM_18",
            "DISPLAY_CS_PIN": "GPIO_NUM_5",
            "DISPLAY_DC_PIN": "GPIO_NUM_4",
            "DISPLAY_WIDTH": "160",
            "DISPLAY_HEIGHT": "128",
            "UP_BUTTON_GPIO": "GPIO_NUM_2",
            "DOWN_BUTTON_GPIO": "GPIO_NUM_13",
            "LEFT_BUTTON_GPIO": "GPIO_NUM_27",
            "RIGHT_BUTTON_GPIO": "GPIO_NUM_35",
            "A_BUTTON_GPIO": "GPIO_NUM_34",
            "B_BUTTON_GPIO": "GPIO_NUM_12",
        }
        self.assertEqual({name: defines[name] for name in expected}, expected)

        gpio_names = [name for name in expected if defines[name].startswith("GPIO_NUM_")]
        audio_gpio_names = [
            "AUDIO_I2S_GPIO_BCLK",
            "AUDIO_I2S_GPIO_WS",
            "AUDIO_I2S_GPIO_DIN",
            "AUDIO_I2S_GPIO_DOUT",
        ]
        pins = [defines[name] for name in gpio_names + audio_gpio_names]
        self.assertEqual(len(pins), len(set(pins)), "Unexpected GPIO collision")

    def test_board_source_preserves_runtime_contracts(self):
        source = (BOARD_DIR / "xueersi_xiaomiao.cc").read_text(encoding="utf-8")
        self.assertEqual(source.count("DECLARE_BOARD(XueersiXiaomiao)"), 1)
        self.assertIn("XiaomiaoAudioCodec", source)
        self.assertIn("esp_lcd_new_panel_st7735", source)
        self.assertIn("new SpiLcdDisplay", source)
        self.assertIn("CreateButton(RIGHT_BUTTON_GPIO, true)", source)
        self.assertIn("CreateButton(A_BUTTON_GPIO, true)", source)
        self.assertGreaterEqual(source.count("Application::GetInstance().Schedule"), 8)
        self.assertGreaterEqual(source.count("kDeviceStateStarting"), 7)

    def test_audio_codec_forces_tx_clock_and_logs_read_path(self):
        codec = (BOARD_DIR / "xiaomiao_audio_codec.cc").read_text(encoding="utf-8")
        header = (BOARD_DIR / "xiaomiao_audio_codec.h").read_text(encoding="utf-8")
        self.assertIn("void XiaomiaoAudioCodec::EnableInput(bool enable)", codec)
        self.assertIn("i2s_channel_enable(tx_handle_)", codec)
        self.assertIn("i2s_channel_read failed", codec)
        self.assertIn("Read() entered", codec)
        self.assertIn("EnableInput(bool enable) override", header)

    def test_build_metadata_targets_classic_esp32_with_psram(self):
        config = json.loads((BOARD_DIR / "config.json").read_text(encoding="utf-8"))
        self.assertEqual(config["type"], "xueersi-xiaomiao")
        self.assertEqual(config["target"], "esp32")
        self.assertEqual([build["name"] for build in config["builds"]], ["xueersi-xiaomiao"])
        options = config["builds"][0]["sdkconfig_append"]
        self.assertIn("CONFIG_SPIRAM=y", options)
        self.assertIn("CONFIG_SPIRAM_USE_MALLOC=y", options)


if __name__ == "__main__":
    unittest.main()
