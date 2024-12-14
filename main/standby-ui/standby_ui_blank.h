/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(STANDBY_BLANK_GUI)
#define STANDBY_BLANK_GUI

#include "globals.h"
#include "lvgl.h"

uint8_t blank_standby_gui_get_id();
const char * blank_standby_gui_get_name();
void blank_standby_gui_init(lv_obj_t *screen);
void blank_standby_gui_cleanup();

#endif
