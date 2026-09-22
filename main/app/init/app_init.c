#include "app_init.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "app_menu.h"
#include "app_page.h"
#include "app_status_bar.h"
#include "app_wifi.h"

void app_init(void)
{
    bsp_display_init();
    bsp_lvgl_init();
    bsp_battery_init();

    bsp_display_backlight(100);

    app_status_bar_init();
    app_page_init();
    app_wifi_init();
    app_menu_init();
}
