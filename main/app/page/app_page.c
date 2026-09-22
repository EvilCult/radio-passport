#include "app_page.h"

#define APP_STATUS_BAR_HEIGHT 32

static lv_obj_t *page;

void app_page_init(void)
{
    page = lv_obj_create(lv_screen_active());

    lv_obj_set_size(
        page,
        LV_PCT(100),
        lv_obj_get_height(lv_screen_active()) - APP_STATUS_BAR_HEIGHT
    );

    lv_obj_set_pos(
        page,
        0,
        APP_STATUS_BAR_HEIGHT
    );

    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
}

void app_page_clear(void)
{
    lv_obj_clean(page);
}

lv_obj_t *app_page_get(void)
{
    return page;
}
