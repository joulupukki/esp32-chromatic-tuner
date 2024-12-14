/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_STANDBY_UI_INTERFACE)
#define TUNER_STANDBY_UI_INTERFACE

#include <stdlib.h>

#include "globals.h"
#include "lvgl.h"

/// @brief Implement a standby UI by implementing this interface.
typedef struct {
    /// @brief Returns a unique ID for the interface.
    ///
    /// Look up other implementations and make sure you don't use an ID that is
    /// already being used. It's a uint8_t so that it can be stored as the
    /// selected ID as a user setting. They must be in consecutive order.
    uint8_t (*get_id)(void);

    /// @brief Returns the name of the standby interface that will be shown in user settings.
    const char * (*get_name)(void);

    /// @brief Initialize the standby UI.
    ///
    /// Interfaces should save a local copy of the screen since it will not be
    /// passed in subsequent calls. The `init()` function will be called at boot
    /// for the active Standby GUI. When the user exits standby mode,
    /// `cleanup()` will be called and `init()` will be called again when
    /// standby mode is activated.
    void (*init)(lv_obj_t *screen);
    
    /// @brief Perform any cleanup needed (this UI is being deactivated).
    ///
    /// The `cleanup()` function is called when the user exits standby mode.
    /// The `init()` function is then called when the user returns to standby.
    ///
    /// The tuner_gui_task takes care of cleaning up the main screen so
    /// Standby GUI interfaces are not responsible for cleaning up the main
    /// screen. The tuner_gui_task will also remove any LVGL objects that were
    /// placed in the screen by the Standby GUI interface.
    void (*cleanup)(void);
} TunerStandbyGUIInterface;

#endif
