/*
 * Copyright (c) 2024 Boyd Timothy. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
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
