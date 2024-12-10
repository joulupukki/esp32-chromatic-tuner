/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_NEEDLE_GUI)
#define TUNER_NEEDLE_GUI

#include "globals.h"
#include "lvgl.h"

uint8_t needle_gui_get_id();
const char * needle_gui_get_name();
void needle_gui_init(lv_obj_t *screen);
void needle_gui_display_frequency(float frequency, TunerNoteName note_name, float cents);
void needle_gui_cleanup();

#endif
