/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "standby_ui_blank.h"

uint8_t blank_standby_gui_get_id() {
    return 0;
}

const char * blank_standby_gui_get_name() {
    return "Blank";
}

void blank_standby_gui_init(lv_obj_t *screen) {
    // Intentionally blank. There's nothing to show on a blank screen. :)

    // TODO: Remove this later before shipping
    lv_obj_t *standby_label = lv_label_create(screen);
    lv_label_set_text_static(standby_label, "Standby (DEBUG)");
    lv_obj_set_style_text_color(standby_label, lv_color_white(), 0);
    lv_obj_center(standby_label);
}

void blank_standby_gui_cleanup() {
    // Intentionally blank. Nothing to clean up.
}
