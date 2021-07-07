#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <sys/util.h>

struct display_data {
	uint8_t id;
	char* data;
};

enum display_data_type {
    DISPLAY_NO_DATA,
    DISPLAY_FOLDER_CHOSEN,
    DISPLAY_PLATFORM_CHOSEN
};

enum btn_id_type {
	BTN_DOWN,
	BTN_UP
};

void lvgl_widgets_init(void);
void hw_button_pressed(uint32_t key_id);
struct display_data hw_button_long_pressed(uint32_t key_id);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
