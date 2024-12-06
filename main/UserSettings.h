/*=============================================================================
    UserSettings.h
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/

#include <vector>
#include <cstdint>
#include <cstring>

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

enum TunerOrientation: std::uint8_t {
    orientationNormal,
    orientationLeft,
    orientationRight,
    orientationUpsideDown,
};

/// @brief A class used to display and manage user settings.
class UserSettings {
    /**
     * @brief Keeps track of the stack of screens currently being displayed.
     * 
     * This class is initialized with the main screen and that screen should
     * always be at position 0 in the stack. As new sub menus are added to
     * the UI, they should be added to `screenStack` and as menus exit, they
     * should be removed. The current screen is always the last item in the
     * `screenStack`.
     */
    std::vector<lv_obj_t*> screenStack;

    // User Setting Variables
    uint8_t             user_in_tune_cents_width    = 2;
    lv_palette_t        user_note_name_color        = LV_PALETTE_NONE; // white. Use lv_color_t c = lv_palette_main(LV_PALETTE_...) to get a color in LVGL. In the case of white, use lv_color_white()
    lv_palette_t        user_pitch_indicator_color  = LV_PALETTE_RED;
    uint8_t             user_pitch_indicator_width  = 8;
    TunerOrientation    user_display_orientation    = orientationNormal;
    uint8_t             user_exp_smoothing          = 0.15f;
    uint8_t             user_1eu_beta               = 0.007f;
    uint8_t             user_note_debounce_interval = 110; // In milliseconds
    uint8_t             user_display_brightness     = 0.75;

    void saveSettings();

    void handleButtonClick(lv_event_t *e);

    void showTunerMenu();
    void showDisplayMenu();
    void showAdvancedMenu();
    void showAboutMenu();
public:
    /**
     * @brief Create the settings object and sets its parameters
     * @param mainScreen The LVGL main screen in the app.
     */
    UserSettings(lv_obj_t * mainScreen);

    /**
     * @brief Pause tuning or standby mode and show the settings menu/screen.
     */
    void showSettings();

    /**
     * @brief Exit the settings menu/screen and resume tuning/standby mode.
     */
    void exitSettings();
};

static void handleTunerButtonClicked(lv_event_t *e);
static void handleInTuneThresholdButtonClicked(lv_event_t *e);
static void handleDisplayButtonClicked(lv_event_t *e);
static void handleBrightnessButtonClicked(lv_event_t *e);
static void handleNoteColorButtonClicked(lv_event_t *e);
static void handleIndicatorButtonClicked(lv_event_t *e);
static void handleRotationButtonClicked(lv_event_t *e);
static void handleDebugButtonClicked(lv_event_t *e);
static void handleExpSmoothingButtonClicked(lv_event_t *e);
static void handle1EUBetaButtonClicked(lv_event_t *e);
static void handle1EUFilterFirstButtonClicked(lv_event_t *e);
static void handleMovingAvgButtonClicked(lv_event_t *e);
static void handleNameDebouncingButtonClicked(lv_event_t *e);
static void handleAboutButtonClicked(lv_event_t *e);
static void handleBackButtonClicked(lv_event_t *e);
static void handleExitButtonClicked(lv_event_t *e);
