#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "application.h"

#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "lamp_controller.h"
#include "led/gpio_led.h"
#include "mcp_server.h"

#include <wifi_station.h>
#include <esp_log.h>

#define TAG "DfrobotFireBeetle2ESP32P4"

class DfrobotFireBeetle2ESP32P4 : public WifiBoard {
private:
    Button boot_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
#if CONFIG_IOT_PROTOCOL_XIAOZHI
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
#elif CONFIG_IOT_PROTOCOL_MCP
        // static LampController lamp(LAMP_GPIO);
#endif
    }

public:
    DfrobotFireBeetle2ESP32P4() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeIot();
        InitializeButtons();
    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 0);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }
};

DECLARE_BOARD(DfrobotFireBeetle2ESP32P4);
