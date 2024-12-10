/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_STROBE_GUI)
#define TUNER_STROBE_GUI

#include "globals.h"
#include "lvgl.h"

uint8_t strobe_gui_get_id();
const char * strobe_gui_get_name();
void strobe_gui_init(lv_obj_t *screen);
void strobe_gui_display_frequency(float frequency, TunerNoteName note_name, float cents);
void strobe_gui_cleanup();

#endif
