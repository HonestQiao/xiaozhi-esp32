#include "k10_lcd_display.h"
#include "display/lvgl_display/jpg/jpeg_to_image.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#define TAG "K10Display"

K10Display::K10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
}

void K10Display::SetOracleImageFromJpeg(const uint8_t* jpeg_data, size_t jpeg_size) {
    uint8_t* out = nullptr;
    size_t out_len = 0, width = 0, height = 0, stride = 0;
    esp_err_t err = jpeg_to_image(jpeg_data, jpeg_size, &out, &out_len, &width, &height, &stride);
    if (err != ESP_OK || out == nullptr) {
        ESP_LOGE(TAG, "Failed to decode JPEG: %s", esp_err_to_name(err));
        return;
    }

    DisplayLockGuard lock(this);

    // Stop auto-hide timer so the image stays visible
    esp_timer_stop(preview_timer_);

    // Stop any GIF animation in emoji_box_
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }

    // Release previous cached image
    oracle_image_cached_.reset();

    // Create LvglImage from decoded data (LvglAllocatedImage takes ownership of out)
    try {
        oracle_image_cached_ = std::make_unique<LvglAllocatedImage>(
            out, out_len, static_cast<int>(width), static_cast<int>(height),
            static_cast<int>(stride), LV_COLOR_FORMAT_RGB565);
    } catch (...) {
        heap_caps_free(out);
        ESP_LOGE(TAG, "Failed to create LvglAllocatedImage");
        return;
    }

    // Set the image source on preview_image_
    lv_image_set_src(preview_image_, oracle_image_cached_->image_dsc());
    if (width > 0 && height > 0) {
        lv_coord_t max_w = width_ / 2;
        lv_coord_t max_h = height_ / 2;
        lv_coord_t zoom_w = (max_w * 256) / width;
        lv_coord_t zoom_h = (max_h * 256) / height;
        lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
        if (zoom > 256) zoom = 256;
        lv_image_set_scale(preview_image_, zoom);
    }

    // Hide emoji box, show preview image
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
}
