#ifndef K10_LCD_DISPLAY_H
#define K10_LCD_DISPLAY_H

#include "display/lcd_display.h"

class K10Display : public SpiLcdDisplay {
private:
    std::unique_ptr<LvglImage> oracle_image_cached_ = nullptr;

public:
    K10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                 int width, int height, int offset_x, int offset_y,
                 bool mirror_x, bool mirror_y, bool swap_xy);

    // Decode JPEG and display on preview_image_
    void SetOracleImageFromJpeg(const uint8_t* jpeg_data, size_t jpeg_size);
};

#endif // K10_LCD_DISPLAY_H
