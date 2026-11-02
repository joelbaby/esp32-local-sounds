// SPDX-License-Identifier: MIT
// Board-only adaptation of XiaoZhi CompactWifiBoard; see LICENSES/XiaoZhi-MIT.txt.
#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "PocketEspAiC3"

class PocketSoundsC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;

    void UseNoDisplay(const char* reason) {
        if (display_ != nullptr) {
            return;
        }
        ESP_LOGW(TAG, "OLED unavailable (%s); continuing without a display", reason);
        display_ = new NoDisplay();
    }

    void InitializeDisplay() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        esp_err_t err = i2c_new_master_bus(&bus_config, &display_i2c_bus_);
        if (err != ESP_OK) {
            UseNoDisplay(esp_err_to_name(err));
            return;
        }

        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3c,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };
        err = esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_);
        if (err != ESP_OK || panel_io_ == nullptr) {
            UseNoDisplay("panel I/O initialization failed");
            return;
        }

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;
        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = DISPLAY_HEIGHT,
        };
        panel_config.vendor_config = &ssd1306_config;

        err = esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_);
        if (err != ESP_OK || panel_ == nullptr) {
            UseNoDisplay("SSD1306 driver initialization failed");
            return;
        }
        if ((err = esp_lcd_panel_reset(panel_)) != ESP_OK ||
            (err = esp_lcd_panel_init(panel_)) != ESP_OK ||
            (err = esp_lcd_panel_invert_color(panel_, false)) != ESP_OK ||
            (err = esp_lcd_panel_disp_on_off(panel_, true)) != ESP_OK) {
            UseNoDisplay(esp_err_to_name(err));
            return;
        }

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                   DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        ESP_LOGI(TAG, "SSD1306 display ready (SDA=20, SCL=10)");
    }

public:
    PocketSoundsC3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplay();
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) EnterWifiConfigMode();
            else app.ToggleChatState();
        });
    }
    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &codec;
    }
    Display* GetDisplay() override { return display_; }
};
DECLARE_BOARD(PocketSoundsC3Board);
