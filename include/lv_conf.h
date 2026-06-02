/* LVGL 8.3 configuration for ESP32-S3 OBD Dashboard */
#if 1

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================  COLOR  ====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP         0
#define LV_COLOR_SCREEN_TRANSP   0
#define LV_COLOR_MIX_ROUND_OFS   0
#define LV_COLOR_CHROMA_KEY      lv_color_hex(0x00ff00)

/*====================  MEMORY  ====================*/
/* Route LVGL allocations to PSRAM so internal RAM isn't exhausted by widget heap */
#define LV_MEM_CUSTOM            1
#define LV_MEM_CUSTOM_INCLUDE    <esp_heap_caps.h>
#define LV_MEM_CUSTOM_ALLOC(size)         heap_caps_malloc(size,      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_CUSTOM_FREE(ptr)           free(ptr)
#define LV_MEM_CUSTOM_REALLOC(ptr, size)  heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_BUF_MAX_NUM       16
#define LV_MEMCPY_MEMSET_STD      0

/*====================  HAL  ====================*/
#define LV_DISP_DEF_REFR_PERIOD  10   /* ms */
#define LV_INDEV_DEF_READ_PERIOD 30   /* ms */

#define LV_TICK_CUSTOM           1
#define LV_TICK_CUSTOM_INCLUDE   <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF               130

/*====================  DRAWING  ====================*/
#define LV_DRAW_COMPLEX          1
#define LV_SHADOW_CACHE_SIZE     0
#define LV_CIRCLE_CACHE_SIZE     4
#define LV_IMG_CACHE_DEF_SIZE    0
#define LV_GRADIENT_MAX_STOPS    2
#define LV_GRAD_CACHE_DEF_SIZE   0
#define LV_DITHER_GRADIENT       0
#define LV_DISP_ROT_MAX_BUF      (10*1024)

/*====================  LOG  ====================*/
#define LV_USE_LOG               0

/*====================  ASSERT  ====================*/
#define LV_USE_ASSERT_NULL       1
#define LV_USE_ASSERT_MALLOC     1
#define LV_USE_ASSERT_STYLE      0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ        0
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER        while(1);

/*====================  COMPILER  ====================*/
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE  4
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD       0

/*====================  FONTS  ====================*/
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    0
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    0
#define LV_FONT_MONTSERRAT_20    0
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    0
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    1
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    1

#define LV_FONT_MONTSERRAT_12_SUBPX       0
#define LV_FONT_MONTSERRAT_28_COMPRESSED  0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW  0
#define LV_FONT_SIMSUN_16_CJK             0
#define LV_FONT_UNSCII_8                  0
#define LV_FONT_UNSCII_16                 0
#define LV_FONT_CUSTOM_DECLARE \
    extern const lv_font_t montserrat_14_cyr; \
    extern const lv_font_t montserrat_16_cyr;

#define LV_FONT_DEFAULT          &montserrat_14_cyr

#define LV_FONT_FMT_TXT_LARGE    0
#define LV_USE_FONT_COMPRESSED   0
#define LV_USE_FONT_SUBPX        0
#define LV_FONT_SUBPX_BGR        0
#define LV_USE_USER_DATA         1

/*====================  TEXT  ====================*/
#define LV_TXT_ENC               LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS       " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD         "#"
#define LV_USE_BIDI              0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================  CORE WIDGETS  ====================*/
#define LV_USE_ARC               1
#define LV_USE_BAR               1
#define LV_USE_BTN               1
#define LV_USE_BTNMATRIX         1
#define LV_USE_CANVAS            0
#define LV_USE_CHECKBOX          0
#define LV_USE_DROPDOWN          0
#define LV_USE_IMG               0
#define LV_USE_LABEL             1
#define LV_LABEL_TEXT_SELECTION  0
#define LV_LABEL_LONG_TXT_HINT   0
#define LV_USE_LINE              0
#define LV_USE_ROLLER            0
#define LV_USE_SLIDER            0
#define LV_USE_SWITCH            0
#define LV_USE_TEXTAREA          0
#define LV_USE_TABLE             0

/*====================  EXTRA WIDGETS  ====================*/
#define LV_USE_ANIMIMG           0
#define LV_USE_CALENDAR          0
#define LV_USE_CHART             0
#define LV_USE_COLORWHEEL        0
#define LV_USE_IMGBTN            0
#define LV_USE_KEYBOARD          0
#define LV_USE_LED               0
#define LV_USE_LIST              1   /* DTC fault list */
#define LV_USE_MENU              0
#define LV_USE_METER             0
#define LV_USE_MSGBOX            0
#define LV_USE_SPAN              0
#define LV_USE_SPINBOX           0
#define LV_USE_SPINNER           0
#define LV_USE_TABVIEW           1   /* 4-tab layout */
#define LV_TABVIEW_DEF_ANIM_TIME 200
#define LV_USE_TILEVIEW          0
#define LV_USE_WIN               0

/*====================  LAYOUTS  ====================*/
#define LV_USE_FLEX              1
#define LV_USE_GRID              0

/*====================  THEMES  ====================*/
#define LV_USE_THEME_DEFAULT     1
#define LV_THEME_DEFAULT_DARK    1
#define LV_THEME_DEFAULT_GROW    0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_SIMPLE      0
#define LV_USE_THEME_MONO        0

/*====================  FILE SYSTEM (off)  ====================*/
#define LV_USE_FS_STDIO          0
#define LV_USE_FS_POSIX          0
#define LV_USE_FS_WIN32          0
#define LV_USE_FS_FATFS          0
#define LV_USE_PNG               0
#define LV_USE_BMP               0
#define LV_USE_SJPG              0
#define LV_USE_GIF               0
#define LV_USE_QRCODE            0
#define LV_USE_FREETYPE          0
#define LV_USE_RLOTTIE           0
#define LV_USE_FFT               0

/*====================  MISC  ====================*/
#define LV_USE_SNAPSHOT          0
#define LV_USE_MONKEY            0
#define LV_USE_GRIDNAV           0
#define LV_USE_FRAGMENT          0
#define LV_USE_IMGFONT           0
#define LV_USE_MSG               0
#define LV_USE_IME_PINYIN        0

#endif /* LV_CONF_H */
#endif /* content enable */
