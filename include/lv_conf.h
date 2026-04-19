/**
 * @file lv_conf.h
 * Minimal LVGL 9.x configuration for NerdDuino Pro (ESP32-2432S028, no PSRAM)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/* Color depth: 16 = RGB565 (matches ILI9341) */
#define LV_COLOR_DEPTH 16

/* Use built-in stdlib */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE    <stdint.h>
#define LV_STDDEF_INCLUDE    <stddef.h>
#define LV_STDBOOL_INCLUDE   <stdbool.h>
#define LV_INTTYPES_INCLUDE  <inttypes.h>
#define LV_LIMITS_INCLUDE    <limits.h>
#define LV_STDARG_INCLUDE    <stdarg.h>

/* 48KB heap for LVGL (ESP32 has 320KB SRAM shared) */
#define LV_MEM_SIZE (48 * 1024U)
#define LV_MEM_POOL_EXPAND_SIZE 0
#define LV_MEM_ADR 0

/* HAL */
#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

/* OS: none (bare metal Arduino) */
#define LV_USE_OS   LV_OS_NONE

/* Drawing */
#define LV_DRAW_BUF_STRIDE_ALIGN    1
#define LV_DRAW_BUF_ALIGN           4
#define LV_DRAW_TRANSFORM_USE_MATRIX 0
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_DRAW_LAYER_MAX_MEMORY    0
#define LV_DRAW_THREAD_STACK_SIZE   (8 * 1024)
#define LV_DRAW_THREAD_PRIO         LV_THREAD_PRIO_HIGH

#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0
    #define LV_DRAW_SW_SUPPORT_RGB565A8     0
    #define LV_DRAW_SW_SUPPORT_RGB888       1
    #define LV_DRAW_SW_SUPPORT_XRGB8888     1
    #define LV_DRAW_SW_SUPPORT_ARGB8888     1
    #define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
    #define LV_DRAW_SW_SUPPORT_L8           1
    #define LV_DRAW_SW_SUPPORT_AL88         1
    #define LV_DRAW_SW_SUPPORT_A8           1
    #define LV_DRAW_SW_SUPPORT_I1           1
    #define LV_DRAW_SW_I1_LUM_THRESHOLD     127
    #define LV_DRAW_SW_DRAW_UNIT_CNT        1
    #define LV_USE_DRAW_ARM2D_SYNC          0
    #define LV_USE_NATIVE_HELIUM_ASM        0
    #define LV_DRAW_SW_COMPLEX              1
    #if LV_DRAW_SW_COMPLEX == 1
        #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
        #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
    #endif
    #define LV_USE_DRAW_SW_ASM              LV_DRAW_SW_ASM_NONE
    #define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
#endif

/* Disable unused GPU backends */
#define LV_USE_NEMA_GFX     0
#define LV_USE_PXP          0
#define LV_USE_G2D          0
#define LV_USE_DRAW_DAVE2D  0
#define LV_USE_DRAW_SDL     0
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_DMA2D   0
#define LV_USE_DRAW_OPENGLES 0
#define LV_USE_PPA          0
#define LV_USE_DRAW_EVE     0
#define LV_USE_DRAW_NANOVG  0

/* Rotation */
#define LV_USE_DRAW_SW_ROTATE   1
#define LV_DISPLAY_ROTATE_0     0
#define LV_DISPLAY_ROTATE_90    1
#define LV_DISPLAY_ROTATE_180   2
#define LV_DISPLAY_ROTATE_270   3

/* Logging */
#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   1
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* Performance monitoring */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* Animation */
#define LV_USE_ANIM     1
#define LV_USE_ANIM_TIMELINE 1

/* Scrolling */
#define LV_USE_SCROLL_ON_FOCUS    1

/* Layers */
#define LV_LAYER_SIMPLE_FALLBACK_BG_COLOR 0x000000

/* Attribute settings */
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN      __attribute__((aligned(4)))
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 4
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Fonts */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_14_CJK            0
#define LV_FONT_SIMSUN_16_CJK            0
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0
#define LV_FONT_CUSTOM_DECLARE
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE   0
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_PLACEHOLDER 1
#define LV_USE_BIDI             0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/* Widgets */
#define LV_USE_ANIMIMG     1
#define LV_USE_ARC         1
#define LV_USE_BAR         1
#define LV_USE_BUTTON      1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CALENDAR    0
#define LV_USE_CANVAS      1
#define LV_USE_CHART       1
#define LV_USE_CHECKBOX    1
#define LV_USE_DROPDOWN    1
#define LV_USE_IMAGE       1
#define LV_USE_IMAGEBUTTON 1
#define LV_USE_KEYBOARD    0
#define LV_USE_LABEL       1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 1
    #define LV_LABEL_WAIT_CHAR_COUNT 3
    #define LV_LABEL_LONG_TXT_HINT  1
#endif
#define LV_USE_LED         1
#define LV_USE_LINE        1
#define LV_USE_LIST        1
#define LV_USE_LOTTIE      0
#define LV_USE_MENU        0
#define LV_USE_METER       0
#define LV_USE_MSGBOX      1
#define LV_USE_ROLLER      1
#define LV_USE_SCALE       1
#define LV_USE_SLIDER      1
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     0
#define LV_USE_SPINNER     1
#define LV_USE_SWITCH      1
#define LV_USE_TABVIEW     1
#define LV_USE_TABLE       1
#define LV_USE_TEXTAREA    1
#if LV_USE_TEXTAREA
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500
#endif
#define LV_USE_TILEVIEW    1
#define LV_USE_WIN         0

/* Themes */
#define LV_USE_THEME_DEFAULT    1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 1
    #define LV_THEME_DEFAULT_GROW 1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
#define LV_USE_THEME_SIMPLE     1
#define LV_USE_THEME_MONO       0

/* Layouts */
#define LV_USE_FLEX     1
#define LV_USE_GRID     1

/* 3rd party libs */
#define LV_USE_LODEPNG  0
#define LV_USE_LIBPNG   0
#define LV_USE_BMP      0
#define LV_USE_TJPGD    0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_GIF      0
#define LV_GIF_CACHE_DECODE_DATA 0
#define LV_USE_BARCODE  0
#define LV_USE_QRCODE   0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_VECTOR_GRAPHIC  0
#define LV_USE_LZ4_INTERNAL    0
#define LV_USE_LZ4_EXTERNAL    0
#define LV_USE_RLE      0
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_RLOTTIE  0
#define LV_USE_FFMPEG   0
#define LV_USE_NANOVG   0
#define LV_USE_OPENGLES 0
#define LV_USE_SDL      0
#define LV_USE_X11      0
#define LV_USE_WINDOWS  0

/* Other */
#define LV_USE_SNAPSHOT     0
#define LV_USE_SYSMON       0
#define LV_USE_PROFILER     0
#define LV_USE_MONKEY       0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_OBSERVER     0
#define LV_USE_IME_PINYIN   0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_FONT_MANAGER  0
#define LV_USE_SVG          0
#define LV_USE_OBJ_ID       0
#define LV_USE_OBJ_ID_BUILTIN 0
#define LV_USE_OBJ_PROPERTY 0
#define LV_USE_OBJ_PROPERTY_NAME 0
#define LV_USE_MATRIX       0
#define LV_USE_FLOAT        0

/* i18n */
#define LV_ENABLE_GLOBAL_CUSTOM 0
#define LV_GLOBAL_CUSTOM_INCLUDE <stdint.h>

/* Build properties */
#define LV_USE_EXAMPLE  0
#define LV_USE_DEMO_WIDGETS          0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK        0
#define LV_USE_DEMO_RENDER           0
#define LV_USE_DEMO_STRESS           0
#define LV_USE_DEMO_MUSIC            0
#define LV_USE_DEMO_FLEX_LAYOUT      0
#define LV_USE_DEMO_MULTILANG        0
#define LV_USE_DEMO_TRANSFORM        0
#define LV_USE_DEMO_SCROLL           0
#define LV_USE_DEMO_VECTOR_GRAPHIC   0

#endif /* LV_CONF_H */
