#ifndef _M5PAPER_DISPLAY_H_
#define _M5PAPER_DISPLAY_H_

#include "lcd_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>

// M5Paper Display Configuration
#define M5PAPER_EPD_WIDTH  960
#define M5PAPER_EPD_HEIGHT 540

/**
 * @brief M5Paper e-ink display driver with LVGL integration using M5Unified
 */
class M5PaperDisplay : public LcdDisplay {
public:
    M5PaperDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y);

    ~M5PaperDisplay();

    // Override LcdDisplay methods
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void ClearChatMessages() override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void SetupUI() override;
    virtual void SetTheme(Theme* theme) override;

    // LVGL Display interface
    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;

    // EPD specific methods
    void DoFullRefresh(bool force = false);
    void EnterLowPowerState();
    void ExitLowPowerState();

protected:
    // LVGL flush callback (static)
    static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);

    // Lock/Unlock for LVGL thread safety
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
    // LVGL display
    lv_display_t* lv_display_;

    // UI Elements
    lv_obj_t* chat_box_;
    lv_obj_t* chat_content_;
    lv_obj_t* emoji_image_;
    lv_obj_t* status_text_;

    // Initialization
    void InitializeM5Unified();
    void InitializeLVGL();

    // LVGL internal flush
    void FlushToEPD(const lv_area_t* area, const uint8_t* px_map);
};

#endif // _M5PAPER_DISPLAY_H_
