/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_UI_INTERFACE)
#define TUNER_UI_INTERFACE

#include <stdlib.h>

#include "lvgl.h"

/// @brief Implement a tuner UI by implementing this interface.
typedef struct {
    /// @brief Returns a unique ID for the interface.
    ///
    /// Look up other implementations and make sure you don't use an ID that is
    /// already being used. It's a uint8_t so that it can be stored as the
    /// selected ID as a user setting. They must be in consecutive order.
    uint8_t (*get_id)(void);

    /// @brief Returns the name of the tuning interface that will be shown in user settings.
    const char * (*get_name)(void);

    /// @brief Initialize the tuner UI.
    ///
    /// Interfaces should save a local copy of the screen since it will not be
    /// passed in subsequent calls. The `init()` function will be called at boot
    /// for the active Tuner GUI. If the user enters user settings, `cleanup()`
    /// will be called and `init()` will be called when user settings exits.
    void (*init)(lv_obj_t *screen);
    
    /// @brief Display the frequency/note/cents/etc.
    void (*display_frequency)(float frequency, const char *note_name, float cents);

    /// @brief Perform any cleanup needed (this UI is being deactivated).
    ///
    /// The `cleanup()` function is called when the user enters user settings.
    /// The `init()` function is then called when the user exits user settings.
    /// If a user chooses a different Tuner GUI in user settings, only that
    /// Tuner GUI will be initialized (and shown).
    ///
    /// The tuner_gui_task takes care of cleaning up the main screen so
    /// Tuner GUI interfaces are not responsible for cleaning up the main
    /// screen. The tuner_gui_task will also remove any LVGL objects that were
    /// placed in the screen by the Tuner GUI interface.
    void (*cleanup)(void);
} TunerGUIInterface;

#endif
