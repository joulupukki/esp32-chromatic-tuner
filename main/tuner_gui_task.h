/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_GUI_TASK)
#define TUNER_GUI_TASK

#include "tuner_controller.h"

void tuner_gui_task_tuner_state_changed(TunerState old_state, TunerState new_state);
void user_settings_updated();

#endif
