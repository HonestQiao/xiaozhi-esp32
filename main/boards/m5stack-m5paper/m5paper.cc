#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "m5paper_display.h"
#include "power_save_timer.h"
#include "mcp_server.h"

#include <M5Unified.h>

#include <esp_log.h>
#include "driver/gpio.h"
// #include "led/circular_strip.h"

#define TAG "M5Paper"

// 自定义禁用内部pull的按钮类
class DisablePullButton : public Button {
public:
    DisablePullButton(gpio_num_t gpio_num) : Button(static_cast<button_handle_t>(nullptr)) {
        if (gpio_num == GPIO_NUM_NC) {
            return;
        }
        // 先删除基类可能创建的句柄
        if (button_handle_) {
            iot_button_delete(button_handle_);
        }

        button_config_t button_config = {
            .long_press_time = 2000,
            .short_press_time = 0
        };
        button_gpio_config_t gpio_config = {
            .gpio_num = gpio_num,
            .active_level = 0,
            .enable_power_save = false,
            .disable_pull = true  // 禁用内部pull-up
        };
        iot_button_new_gpio_device(&button_config, &gpio_config, &button_handle_);
    }
};

class M5Paper : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    DisablePullButton boot_button_;
    DisablePullButton volume_up_button_;
    DisablePullButton volume_down_button_;
    M5PaperDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    Es8311AudioCodec* audio_codec_;

    void InitializePowerSaveTimer() {
        // cpu_max_freq=240: 进入睡眠时降低 CPU 频率到 80MHz
        // seconds_to_sleep=60: 闲置 60 秒后进入睡眠模式
        // seconds_to_shutdown=-1: 不自动关机（电子纸可以长时间显示）
        power_save_timer_ = new PowerSaveTimer(240, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Entering sleep mode");
            // 停止音频输入以省电
            if (audio_codec_) {
                audio_codec_->EnableInput(false);
            }
            // 设置电子纸为省电模式（如果支持）
            display_->EnterLowPowerState();
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting sleep mode");
            // 恢复音频输入
            if (audio_codec_) {
                audio_codec_->EnableInput(true);
            }
            // 唤醒电子纸
            display_->ExitLowPowerState();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        // 使用 I2C_NUM_0 驱动 ES8311（GPIO 25/32），避免与 M5Unified 的 I2C_NUM_1（GPIO 21/22 触摸屏/RTC）冲突
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)EXT_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 0,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnLongPress([this]() {
            // gpio_set_level(EPD_MAIN_PWR_PIN, false);
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
        });
    }

    void InitializeGpio() {
        ESP_LOGI(TAG, "=== GPIO Reset Debug ===");

        ESP_LOGI(TAG, "EPD_MAIN_PWR_PIN = %d", (int)EPD_MAIN_PWR_PIN);
        gpio_reset_pin(EPD_MAIN_PWR_PIN);
        gpio_set_direction(EPD_MAIN_PWR_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(EPD_MAIN_PWR_PIN, true);

        ESP_LOGI(TAG, "EXT_PWR_EN_GPIO = %d", (int)EXT_PWR_EN_GPIO);
        gpio_reset_pin(EXT_PWR_EN_GPIO);
        gpio_set_direction(EXT_PWR_EN_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(EXT_PWR_EN_GPIO, true);

        // 释放 GPIO 25/32，解除其他外设占用
        ESP_LOGI(TAG, "AUDIO_CODEC_I2C_SDA_PIN = %d", (int)AUDIO_CODEC_I2C_SDA_PIN);
        gpio_reset_pin(AUDIO_CODEC_I2C_SDA_PIN);
        ESP_LOGI(TAG, "AUDIO_CODEC_I2C_SCL_PIN = %d", (int)AUDIO_CODEC_I2C_SCL_PIN);
        gpio_reset_pin(AUDIO_CODEC_I2C_SCL_PIN);

        // 释放 I2S 引脚
        // ESP_LOGI(TAG, "AUDIO_I2S_GPIO_MCLK = %d", (int)AUDIO_I2S_GPIO_MCLK);
        // gpio_reset_pin(AUDIO_I2S_GPIO_MCLK);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_BCLK = %d", (int)AUDIO_I2S_GPIO_BCLK);
        gpio_reset_pin(AUDIO_I2S_GPIO_BCLK);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_DOUT = %d", (int)AUDIO_I2S_GPIO_DOUT);
        gpio_reset_pin(AUDIO_I2S_GPIO_DOUT);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_DIN = %d", (int)AUDIO_I2S_GPIO_DIN);
        gpio_reset_pin(AUDIO_I2S_GPIO_DIN);

        // 释放 EPD 引脚（仅 MOSI/MISO/SCK，不碰 RST/CS/BUSY）
        // RST/CS/BUSY 由 IT8951EPD 驱动自行管理
        ESP_LOGI(TAG, "EPD_GPIO_MOSI = %d", (int)EPD_GPIO_MOSI);
        gpio_reset_pin(EPD_GPIO_MOSI);
        ESP_LOGI(TAG, "EPD_GPIO_MISO = %d", (int)EPD_GPIO_MISO);
        gpio_reset_pin(EPD_GPIO_MISO);
        ESP_LOGI(TAG, "EPD_GPIO_SCK = %d", (int)EPD_GPIO_SCK);
        gpio_reset_pin(EPD_GPIO_SCK);
        // 不 reset EPD_GPIO_CS, EPD_GPIO_RST, EPD_GPIO_BUSY

        ESP_LOGI(TAG, "=== GPIO Reset Complete ===");
    }

    void InitializeEpdDisplay() {
        ESP_LOGI(TAG, "Creating M5Paper EPD display");

        // Create display - M5Unified handles IT8951 internally
        display_ = new M5PaperDisplay(
            nullptr,  // panel_io - not used for IT8951
            nullptr,  // panel - not used for IT8951
            EPD_WIDTH,
            EPD_HEIGHT,
            0,  // offset_x
            0   // offset_y
        );
    }

    void InitializeMcpTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.environment.get_temperature",
            "获取环境温度和湿度。返回当前环境的温度（摄氏度）和相对湿度。",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                float temp = ReadSht30Temperature();
                if (temp < -900.0f) {
                    return std::string("Failed to read SHT30 sensor");
                }

                // 读取湿度
                uint8_t data[6];
                M5.In_I2C.start(0x44, false, 400000);
                M5.In_I2C.write(0x2C);
                M5.In_I2C.write(0x06);
                M5.In_I2C.stop();
                vTaskDelay(pdMS_TO_TICKS(20));
                M5.In_I2C.start(0x44, true, 400000);
                M5.In_I2C.read(data, 6, true);
                M5.In_I2C.stop();

                float humidity = 0.0f;
                if (Sht30Crc8(data + 3, 2) == data[5]) {
                    uint16_t raw_humi = (data[3] << 8) | data[4];
                    humidity = 100.0f * raw_humi / 65535.0f;
                }

                cJSON* result = cJSON_CreateObject();
                cJSON_AddNumberToObject(result, "temperature", temp);
                cJSON_AddNumberToObject(result, "humidity", humidity);
                cJSON_AddStringToObject(result, "unit_temp", "celsius");
                cJSON_AddStringToObject(result, "unit_humidity", "percent");
                return result;
            });
    }

    // SHT30 CRC8 校验辅助函数
    uint8_t Sht30Crc8(const uint8_t* data, int len) {
        uint8_t crc = 0xFF;
        for (int i = 0; i < len; i++) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 0x80) crc = (crc << 1) ^ 0x31;
                else crc = (crc << 1);
            }
        }
        return crc;
    }

    // 读取 SHT30 温度
    float ReadSht30Temperature() {
        uint8_t data[6];

        // 使用 M5Unified 的内部 I2C 总线 (GPIO 21/22)
        // 发送测量命令 0x2C06 (高重复性)
        M5.In_I2C.start(0x44, false, 400000);
        M5.In_I2C.write(0x2C);
        M5.In_I2C.write(0x06);
        M5.In_I2C.stop();

        // 等待测量完成 (SHT30 高重复性约需 15ms)
        vTaskDelay(pdMS_TO_TICKS(20));

        // 读取 6 字节数据 (Temp MSB, LSB, CRC, Humi MSB, LSB, CRC)
        M5.In_I2C.start(0x44, true, 400000);
        M5.In_I2C.read(data, 6, true);  // last_nack=true
        M5.In_I2C.stop();

        // CRC 校验
        if (Sht30Crc8(data, 2) != data[2]) return -999.0f;

        // 计算温度
        uint16_t raw_temp = (data[0] << 8) | data[1];
        return -45.0f + (175.0f * raw_temp / 65535.0f);
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

public:
    M5Paper() : boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        audio_codec_(nullptr) {
        ESP_LOGI(TAG, "=== M5Paper GPIO Config ===");
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_MCLK: %d", (int)AUDIO_I2S_GPIO_MCLK);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_BCLK: %d", (int)AUDIO_I2S_GPIO_BCLK);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_WS: %d", (int)AUDIO_I2S_GPIO_WS);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_DOUT: %d", (int)AUDIO_I2S_GPIO_DOUT);
        ESP_LOGI(TAG, "AUDIO_I2S_GPIO_DIN: %d", (int)AUDIO_I2S_GPIO_DIN);
        ESP_LOGI(TAG, "AUDIO_CODEC_I2C_SDA_PIN: %d", (int)AUDIO_CODEC_I2C_SDA_PIN);
        ESP_LOGI(TAG, "AUDIO_CODEC_I2C_SCL_PIN: %d", (int)AUDIO_CODEC_I2C_SCL_PIN);
        ESP_LOGI(TAG, "AUDIO_CODEC_ES8311_ADDR: 0x%02x", AUDIO_CODEC_ES8311_ADDR);
        ESP_LOGI(TAG, "AUDIO_CODEC_GPIO_PA: %d", (int)AUDIO_CODEC_GPIO_PA);
        ESP_LOGI(TAG, "BUILTIN_LED_GPIO: %d", (int)BUILTIN_LED_GPIO);
        ESP_LOGI(TAG, "BOOT_BUTTON_GPIO: %d", (int)BOOT_BUTTON_GPIO);
        ESP_LOGI(TAG, "VOLUME_UP_BUTTON_GPIO: %d", (int)VOLUME_UP_BUTTON_GPIO);
        ESP_LOGI(TAG, "VOLUME_DOWN_BUTTON_GPIO: %d", (int)VOLUME_DOWN_BUTTON_GPIO);
        ESP_LOGI(TAG, "EPD_MAIN_PWR_PIN: %d", (int)EPD_MAIN_PWR_PIN);
        ESP_LOGI(TAG, "============================\r\n");
        // gpio_dump_io_configuration(stdout, (1ULL << 25) | (1ULL << 32));

        InitializeGpio();
        InitializeButtons();
        InitializeI2c();
        I2cDetect();
        InitializeEpdDisplay();
        InitializeMcpTools();
        InitializePowerSaveTimer();
    }

    // virtual Led* GetLed() override {
    //     static CircularStrip led(BUILTIN_LED_GPIO, 25);
    //     return &led;
    // }

    virtual AudioCodec* GetAudioCodec() override {
        if (audio_codec_ == nullptr) {
            audio_codec_ = new Es8311AudioCodec(
                i2c_bus_,
                EXT_I2C_NUM,  // 使用 I2C_NUM_0 对应 GPIO 25/32 (PORT_A)
                AUDIO_INPUT_SAMPLE_RATE,
                AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK,
                AUDIO_I2S_GPIO_BCLK,
                AUDIO_I2S_GPIO_WS,
                AUDIO_I2S_GPIO_DOUT,
                AUDIO_I2S_GPIO_DIN,
                AUDIO_CODEC_GPIO_PA,
                AUDIO_CODEC_ES8311_ADDR,
                false);
        }
        return audio_codec_;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        // 使用 M5Unified 的电源管理接口获取电池信息
        level = M5.Power.getBatteryLevel();

        // 获取充电状态
        auto charge_status = M5.Power.isCharging();
        charging = (charge_status == m5::Power_Class::is_charging_t::is_charging);
        discharging = (charge_status == m5::Power_Class::is_charging_t::is_discharging);

        return true;
    }
};

DECLARE_BOARD(M5Paper);
