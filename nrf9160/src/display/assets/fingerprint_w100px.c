#include <lvgl.h>

#if CONFIG_LVGL_USE_IMG
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
#ifndef LV_ATTRIBUTE_IMG_FINGERPRINT_W100PX
#define LV_ATTRIBUTE_IMG_FINGERPRINT_W100PX
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_IMG_FINGERPRINT_W100PX uint8_t fingerprint_w100px_map[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x07, 0xff, 0x80, 0x00, 0x1f, 0xfe, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x07, 0xe0, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x1f, 0xc0, 0x00, 0xff, 0xff, 0xf0, 0x00, 0x3f, 0x80, 0x00, 0x00, 
  0x00, 0x00, 0x3f, 0x00, 0x07, 0xff, 0xff, 0xff, 0x00, 0x0f, 0xc0, 0x00, 0x00, 
  0x00, 0x00, 0x7e, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xc0, 0x07, 0xe0, 0x00, 0x00, 
  0x00, 0x00, 0xf8, 0x00, 0x7f, 0xf0, 0x00, 0x7f, 0xf0, 0x01, 0xf0, 0x00, 0x00, 
  0x00, 0x01, 0xf0, 0x01, 0xff, 0x00, 0x00, 0x07, 0xfc, 0x00, 0xf8, 0x00, 0x00, 
  0x00, 0x03, 0xe0, 0x07, 0xf8, 0x00, 0x00, 0x00, 0xff, 0x00, 0x7c, 0x00, 0x00, 
  0x00, 0x07, 0xc0, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x3f, 0x80, 0x3e, 0x00, 0x00, 
  0x00, 0x0f, 0x80, 0x3f, 0x80, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x1f, 0x00, 0x00, 
  0x00, 0x1f, 0x00, 0x7e, 0x00, 0x00, 0xf0, 0x00, 0x03, 0xf0, 0x0f, 0x80, 0x00, 
  0x00, 0x3e, 0x01, 0xfc, 0x00, 0x3f, 0xff, 0xc0, 0x01, 0xf8, 0x07, 0xc0, 0x00, 
  0x00, 0x3c, 0x03, 0xf0, 0x01, 0xff, 0xff, 0xf8, 0x00, 0xfc, 0x03, 0xc0, 0x00, 
  0x00, 0x78, 0x07, 0xe0, 0x0f, 0xff, 0xff, 0xfc, 0x00, 0x3e, 0x01, 0xe0, 0x00, 
  0x00, 0xf8, 0x0f, 0xc0, 0x3f, 0xf8, 0x03, 0xfe, 0x00, 0x1f, 0x01, 0xf0, 0x00, 
  0x00, 0xf0, 0x1f, 0x80, 0x7f, 0x80, 0x00, 0x1c, 0x00, 0x0f, 0x80, 0xf0, 0x00, 
  0x01, 0xe0, 0x3e, 0x01, 0xfe, 0x00, 0x00, 0x00, 0x38, 0x07, 0xc0, 0x78, 0x00, 
  0x03, 0xe0, 0x3e, 0x03, 0xf8, 0x00, 0x00, 0x00, 0x7c, 0x03, 0xe0, 0x7c, 0x00, 
  0x03, 0xc0, 0x7c, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x3e, 0x01, 0xe0, 0x3c, 0x00, 
  0x07, 0xc0, 0xf8, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x3f, 0x01, 0xf0, 0x3e, 0x00, 
  0x07, 0x81, 0xf0, 0x1f, 0x00, 0x0f, 0xff, 0x00, 0x0f, 0x80, 0xf8, 0x1e, 0x00, 
  0x0f, 0x01, 0xe0, 0x3e, 0x00, 0x7f, 0xff, 0xe0, 0x07, 0xc0, 0x78, 0x0f, 0x00, 
  0x0f, 0x03, 0xc0, 0x7c, 0x01, 0xff, 0xff, 0xf8, 0x03, 0xe0, 0x3c, 0x0f, 0x00, 
  0x0f, 0x03, 0xc0, 0xf8, 0x03, 0xff, 0x0f, 0xfc, 0x01, 0xe0, 0x3c, 0x0f, 0x00, 
  0x1e, 0x07, 0x80, 0xf0, 0x0f, 0xf0, 0x00, 0xff, 0x00, 0xf0, 0x1e, 0x07, 0x80, 
  0x1e, 0x07, 0x81, 0xe0, 0x1f, 0x80, 0x00, 0x1f, 0x80, 0xf8, 0x1e, 0x07, 0x80, 
  0x3c, 0x0f, 0x03, 0xe0, 0x3f, 0x00, 0x00, 0x0f, 0xc0, 0x78, 0x0f, 0x03, 0xc0, 
  0x3c, 0x0f, 0x03, 0xc0, 0x7c, 0x00, 0x00, 0x03, 0xe0, 0x3c, 0x0f, 0x03, 0xc0, 
  0x3c, 0x1e, 0x07, 0xc0, 0xf8, 0x00, 0x00, 0x01, 0xf0, 0x3c, 0x07, 0x83, 0xc0, 
  0x38, 0x1e, 0x07, 0x80, 0xf0, 0x03, 0xfc, 0x00, 0xf0, 0x1e, 0x07, 0x81, 0xc0, 
  0x78, 0x1c, 0x07, 0x81, 0xe0, 0x0f, 0xff, 0x00, 0x78, 0x1e, 0x07, 0x81, 0xe0, 
  0x78, 0x3c, 0x0f, 0x03, 0xe0, 0x3f, 0xff, 0xc0, 0x7c, 0x0f, 0x03, 0x81, 0xe0, 
  0x78, 0x3c, 0x0f, 0x03, 0xc0, 0x7f, 0xff, 0xe0, 0x3c, 0x0f, 0x03, 0xc1, 0xe0, 
  0x70, 0x3c, 0x0e, 0x07, 0xc0, 0xfc, 0x03, 0xf0, 0x3e, 0x0f, 0x03, 0xc0, 0xe0, 
  0x70, 0x38, 0x1e, 0x07, 0x81, 0xf8, 0x00, 0xf8, 0x1e, 0x07, 0x83, 0xc0, 0xe0, 
  0xf0, 0x78, 0x1e, 0x07, 0x81, 0xe0, 0x00, 0x78, 0x1e, 0x07, 0x81, 0xc0, 0xf0, 
  0xf0, 0x78, 0x1e, 0x0f, 0x03, 0xe0, 0x00, 0x3c, 0x0c, 0x07, 0x81, 0xc0, 0xf0, 
  0xf0, 0x78, 0x1e, 0x0f, 0x03, 0xc0, 0x00, 0x3c, 0x00, 0x07, 0x81, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0f, 0x03, 0x80, 0x60, 0x1e, 0x00, 0x03, 0x81, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0f, 0x07, 0x80, 0xf0, 0x1e, 0x00, 0x03, 0x81, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0f, 0x07, 0x80, 0xf0, 0x1e, 0x06, 0x03, 0x81, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0e, 0x07, 0x80, 0xf0, 0x1e, 0x07, 0x03, 0x81, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0f, 0x07, 0x80, 0x70, 0x1e, 0x07, 0x03, 0xc1, 0xe0, 0xf0, 
  0xf0, 0x78, 0x1c, 0x0f, 0x07, 0x80, 0x70, 0x1e, 0x07, 0x83, 0xc1, 0xe0, 0x60, 
  0xf0, 0x38, 0x1c, 0x0f, 0x07, 0x80, 0x78, 0x1e, 0x07, 0x83, 0xc1, 0xe0, 0x00, 
  0xf0, 0x38, 0x1c, 0x0f, 0x07, 0x80, 0x78, 0x1e, 0x07, 0x83, 0xc1, 0xe0, 0x00, 
  0xf0, 0x38, 0x1c, 0x0f, 0x07, 0x80, 0x78, 0x1e, 0x07, 0x83, 0xc1, 0xe0, 0x00, 
  0xf0, 0x3c, 0x1c, 0x0f, 0x07, 0x80, 0x78, 0x1e, 0x07, 0x83, 0xc1, 0xe0, 0x00, 
  0xf0, 0x3c, 0x1c, 0x0f, 0x03, 0x80, 0x78, 0x1e, 0x03, 0x83, 0xc1, 0xe0, 0x00, 
  0x70, 0x3c, 0x1e, 0x0f, 0x03, 0x80, 0x78, 0x1e, 0x03, 0x83, 0xc1, 0xe0, 0x00, 
  0x70, 0x3c, 0x1e, 0x0f, 0x03, 0x80, 0x78, 0x1e, 0x03, 0x83, 0xc1, 0xe0, 0x00, 
  0x78, 0x3c, 0x1e, 0x0f, 0x03, 0x80, 0x78, 0x1e, 0x03, 0xc3, 0xc1, 0xe0, 0x00, 
  0x78, 0x3c, 0x1e, 0x07, 0x03, 0x80, 0x78, 0x1e, 0x03, 0xc3, 0xc0, 0xc0, 0x00, 
  0x78, 0x3c, 0x1e, 0x07, 0x03, 0x80, 0x78, 0x1e, 0x03, 0xc3, 0xc0, 0x00, 0x00, 
  0x38, 0x3c, 0x1e, 0x07, 0x03, 0x80, 0x78, 0x1e, 0x03, 0xc3, 0xc0, 0x00, 0x00, 
  0x3c, 0x3c, 0x1e, 0x07, 0x03, 0x80, 0x78, 0x1e, 0x03, 0xc3, 0xc0, 0x00, 0x00, 
  0x3c, 0x3c, 0x0e, 0x07, 0x03, 0x80, 0x78, 0x0c, 0x03, 0xc3, 0xc1, 0xc0, 0x00, 
  0x3c, 0x3c, 0x0e, 0x07, 0x03, 0x80, 0x78, 0x00, 0x03, 0xc3, 0xc1, 0xe0, 0x00, 
  0x1e, 0x3c, 0x0e, 0x07, 0x03, 0x80, 0x78, 0x00, 0x03, 0xc3, 0xc1, 0xe0, 0x00, 
  0x1e, 0x3c, 0x0f, 0x07, 0x03, 0x80, 0x78, 0x00, 0x03, 0xc3, 0xc1, 0xe0, 0x00, 
  0x0c, 0x3c, 0x0f, 0x07, 0x03, 0x80, 0x78, 0x00, 0x03, 0xc3, 0xc1, 0xc0, 0x00, 
  0x00, 0x78, 0x0f, 0x0f, 0x03, 0x80, 0x70, 0x00, 0x03, 0xc3, 0xc1, 0xc0, 0x00, 
  0x00, 0x78, 0x0f, 0x0f, 0x03, 0x80, 0xf0, 0x0c, 0x03, 0xc3, 0xc1, 0xc0, 0x00, 
  0x00, 0xf8, 0x0f, 0x0f, 0x03, 0x80, 0xf0, 0x1e, 0x03, 0xc3, 0xc1, 0xc0, 0x00, 
  0x00, 0xf0, 0x0e, 0x0f, 0x07, 0x80, 0xf0, 0x1e, 0x03, 0xc3, 0xc3, 0xc0, 0x00, 
  0x00, 0xf0, 0x0e, 0x0f, 0x07, 0x80, 0xf0, 0x1e, 0x03, 0xc3, 0xc3, 0xc0, 0x00, 
  0x00, 0xe0, 0x1e, 0x0f, 0x07, 0x80, 0xe0, 0x1e, 0x03, 0xc3, 0xc3, 0xc0, 0x00, 
  0x00, 0xc0, 0x1e, 0x0f, 0x07, 0x81, 0xe0, 0x1e, 0x03, 0xc3, 0xc3, 0xc0, 0x00, 
  0x00, 0x00, 0x1e, 0x0f, 0x07, 0x81, 0xe0, 0x1e, 0x03, 0x83, 0x83, 0xc0, 0x00, 
  0x00, 0x00, 0x3c, 0x0f, 0x07, 0x81, 0xe0, 0x1e, 0x03, 0x83, 0x83, 0xc0, 0x00, 
  0x00, 0x00, 0x7c, 0x0e, 0x07, 0x81, 0xc0, 0x1c, 0x07, 0x83, 0x83, 0xc0, 0x00, 
  0x00, 0x00, 0x78, 0x04, 0x07, 0x03, 0xc0, 0x3c, 0x07, 0x87, 0x83, 0x80, 0x00, 
  0x00, 0x01, 0xf8, 0x00, 0x07, 0x01, 0xc0, 0x3c, 0x07, 0x87, 0x80, 0x00, 0x00, 
  0x00, 0x03, 0xf0, 0x00, 0x0f, 0x00, 0x00, 0x3c, 0x07, 0x87, 0x80, 0x00, 0x00, 
  0x00, 0x07, 0xe0, 0x00, 0x0f, 0x00, 0x00, 0x3c, 0x07, 0x87, 0x80, 0x00, 0x00, 
  0x00, 0x07, 0xc0, 0x18, 0x0f, 0x00, 0x00, 0x3c, 0x07, 0x07, 0x80, 0x00, 0x00, 
  0x00, 0x03, 0x00, 0x38, 0x0e, 0x00, 0x00, 0x78, 0x0f, 0x07, 0x80, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x7c, 0x1e, 0x06, 0x00, 0x78, 0x0f, 0x07, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x78, 0x1e, 0x0f, 0x00, 0x78, 0x0f, 0x0f, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0xf0, 0x1e, 0x0f, 0x00, 0x78, 0x1e, 0x0f, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x01, 0xf0, 0x3c, 0x0f, 0x00, 0xf0, 0x1e, 0x0f, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x01, 0xe0, 0x3c, 0x1e, 0x00, 0xf0, 0x1e, 0x0e, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xc0, 0x7c, 0x1e, 0x00, 0xf0, 0x3c, 0x04, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xc0, 0x78, 0x3c, 0x01, 0xe0, 0x3c, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0xf8, 0x3c, 0x01, 0xe0, 0x3c, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x3c, 0x01, 0xe0, 0x78, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0xe0, 0x78, 0x03, 0xc0, 0x78, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0xe0, 0x78, 0x03, 0xc0, 0x70, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

const lv_img_dsc_t fingerprint_w100px = {
  .header.always_zero = 0,
  .header.w = 100,
  .header.h = 100,
  .data_size = 1301,
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .data = fingerprint_w100px_map,
};

#endif //CONFIG_LVGL_USE_IMG