#include "SplashScreen.h"
#include <lvgl.h>
#include <cstring>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREY    lv_color_hex(0x94a3b8)
#define COL_SUBTLE  lv_color_hex(0x4a5568)

static lv_obj_t* s_scr     = nullptr;
static lv_obj_t* s_lblStat = nullptr;

namespace SplashScreen {

void create()
{
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Title — large orange
    lv_obj_t* title = lv_label_create(s_scr);
    lv_label_set_text(title, "NerdDuino Pro");
    lv_obj_set_style_text_color(title, COL_ORANGE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -55);

    // Tagline under title
    lv_obj_t* tag = lv_label_create(s_scr);
    lv_label_set_text(tag, "Dual-coin miner  .  DUCO + BTC");
    lv_obj_set_style_text_color(tag, COL_SUBTLE, 0);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_14, 0);
    lv_obj_align(tag, LV_ALIGN_CENTER, 0, -20);

    // Spinner
    lv_obj_t* sp = lv_spinner_create(s_scr);
    lv_obj_set_size(sp, 36, 36);
    lv_obj_align(sp, LV_ALIGN_CENTER, 0, 28);
    lv_obj_set_style_arc_color(sp, COL_SUBTLE, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, COL_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);

    // Status line — mutated during boot
    s_lblStat = lv_label_create(s_scr);
    lv_label_set_text(s_lblStat, "Iniciando...");
    lv_obj_set_style_text_color(s_lblStat, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblStat, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblStat, LV_ALIGN_CENTER, 0, 70);
}

void load()
{
    if (!s_scr) return;
    lv_screen_load(s_scr);
}

void setStatus(const char* text)
{
    if (!s_lblStat || !text) return;
    lv_label_set_text(s_lblStat, text);
    // Caller is responsible for pumping LVGL (UIManager::pumpLvgl) so the
    // text actually reaches the panel before they do blocking work.
}

} // namespace SplashScreen
