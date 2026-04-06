#include "m5paper_display.h"
#include "lvgl_theme.h"
#include "assets/lang_config.h"
#include "lvgl_image.h"
#include "lvgl_font.h"

#include <M5Unified.h>

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_lvgl_port.h>

#define TAG "M5PaperDisplay"

#define LVGL_DISP_BUF_SIZE (M5PAPER_EPD_WIDTH * 10)

M5PaperDisplay::M5PaperDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                               int width, int height, int offset_x, int offset_y)
    : LcdDisplay(panel_io, panel, width, height),
      lv_display_(nullptr),
      chat_box_(nullptr), chat_content_(nullptr), emoji_image_(nullptr), status_text_(nullptr) {

    ESP_LOGI(TAG, "=== M5Paper Display Initialization ===");
    ESP_LOGI(TAG, "Resolution: %dx%d", width, height);

    InitializeM5Unified();
    InitializeLVGL();
}

M5PaperDisplay::~M5PaperDisplay() {
}

void M5PaperDisplay::InitializeM5Unified() {
    ESP_LOGI(TAG, "Initializing M5Unified for M5Paper");

    // Configure M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.output_power  = true;
    cfg.internal_imu  = true;
    cfg.internal_rtc  = true;
    cfg.internal_spk  = true;
    cfg.internal_mic  = true;

    // Begin M5Unified - this auto-detects M5Paper and initializes IT8951
    M5.begin(cfg);

    // Set EPD mode for fastest refresh
    M5.Display.setEpdMode(m5gfx::epd_mode_t::epd_fastest);

    // Set portrait orientation (540x960)
    M5.Display.setRotation(0);

    ESP_LOGI(TAG, "M5Unified initialized, display: %ldx%ld",
             (long)M5.Display.width(), (long)M5.Display.height());
}

void M5PaperDisplay::InitializeLVGL() {
    ESP_LOGI(TAG, "Initializing LVGL");

    // Initialize LVGL port
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 5;
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    // Lock LVGL
    lvgl_port_lock(0);

    // 动态获取 M5Unified 设置的显示尺寸（竖屏时为 540x960，横屏时为 960x540）
    int width = M5.Display.width();
    int height = M5.Display.height();
    ESP_LOGI(TAG, "Creating LVGL display: %dx%d", width, height);

    // 创建 LVGL 显示
    lv_display_ = lv_display_create(width, height);

    // 电子纸必须使用 FULL 模式，否则 PARTIAL 刷新会导致旧内容残留和重影
    // 分配完整屏幕的 buffer (RGB565)
    size_t buf_size = width * height * sizeof(lv_color_t);
    void* buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf1) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffer (%lu bytes)", (unsigned long)buf_size);
        lvgl_port_unlock();
        return;
    }

    lv_display_set_buffers(lv_display_, buf1, nullptr, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(lv_display_, lvgl_flush_cb);
    lv_display_set_user_data(lv_display_, this);

    // Initialize themes
    InitializeLcdThemes();

    // Create themes and setup UI
    SetupUI();

    // Initial EPD clear and full refresh
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.display();

    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL initialized");
}

void M5PaperDisplay::lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    M5PaperDisplay* driver = (M5PaperDisplay*)lv_display_get_user_data(disp);
    if (driver) {
        driver->FlushToEPD(area, px_map);
    }
}

void M5PaperDisplay::FlushToEPD(const lv_area_t* area, const uint8_t* px_map) {
    // In FULL mode, area covers the entire screen
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2;
    int y2 = area->y2;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;

    if (w <= 0 || h <= 0) {
        lv_disp_flush_ready(lv_display_);
        return;
    }

    // Push the full frame to M5GFX (handles RGB565 → 4-bit grayscale internally)
    M5.Display.pushImage(x1, y1, w, h, (const uint16_t*)px_map);

    // Trigger EPD refresh to actually display the content
    // This is critical for e-ink: without display(), the image stays in the internal buffer
    M5.Display.display();

    // Signal flush complete
    lv_disp_flush_ready(lv_display_);
}

void M5PaperDisplay::DoFullRefresh(bool force) {
    // FlushToEPD already calls M5.Display.display(), so this is just a safety call
    M5.Display.display();
}

void M5PaperDisplay::EnterLowPowerState() {
    ESP_LOGI(TAG, "Display entering low power state");
    // IT8951 supports standby mode - keeps the image on screen but stops refreshing
    M5.Display.sleep();
}

void M5PaperDisplay::ExitLowPowerState() {
    ESP_LOGI(TAG, "Display exiting low power state");
    // Wake up IT8951 - it needs some time to recover from sleep
    M5.Display.wakeup();
    vTaskDelay(pdMS_TO_TICKS(100));
    // Do a full refresh to clear any ghosting
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.display();
}

bool M5PaperDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void M5PaperDisplay::Unlock() {
    lvgl_port_unlock();
}

void M5PaperDisplay::SetupUI() {
    // Call parent setup
    LcdDisplay::SetupUI();

    // E-ink optimized UI: disable animations, clear contrast
    lv_obj_set_style_bg_color(container_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_color(container_, lv_color_hex(0x000000), LV_PART_MAIN);
}

void M5PaperDisplay::SetEmotion(const char* emotion) {
    if (!Lock(1000)) {
        return;
    }
    LcdDisplay::SetEmotion(emotion);
    Unlock();
}

void M5PaperDisplay::SetChatMessage(const char* role, const char* content) {
    if (!Lock(1000)) {
        return;
    }
    LcdDisplay::SetChatMessage(role, content);
    Unlock();
}

void M5PaperDisplay::ClearChatMessages() {
    if (!Lock(1000)) {
        return;
    }
    LcdDisplay::ClearChatMessages();
    Unlock();
}

void M5PaperDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    if (!Lock(1000)) {
        return;
    }
    LcdDisplay::SetPreviewImage(std::move(image));
    DoFullRefresh();
    Unlock();
}

void M5PaperDisplay::SetTheme(Theme* theme) {
    if (!Lock(1000)) {
        return;
    }
    LcdDisplay::SetTheme(theme);
    DoFullRefresh();
    Unlock();
}

void M5PaperDisplay::SetStatus(const char* status) {
    if (!Lock(1000)) {
        return;
    }
    LvglDisplay::SetStatus(status);
    Unlock();
}

void M5PaperDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (!Lock(1000)) {
        return;
    }
    LvglDisplay::ShowNotification(notification, duration_ms);
    Unlock();
}

void M5PaperDisplay::ShowNotification(const std::string& notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void M5PaperDisplay::UpdateStatusBar(bool update_all) {
    if (!Lock(1000)) {
        return;
    }
    LvglDisplay::UpdateStatusBar(update_all);
    Unlock();
}
