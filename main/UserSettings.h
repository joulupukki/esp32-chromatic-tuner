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

extern "C" { // because these files are C and not C++
    #include "lcd.h"
}

#include "nvs_flash.h"
#include "nvs.h"

#define DEFAULT_IN_TUNE_CENTS_WIDTH     ((uint8_t) 2)
#define DEFAULT_NOTE_NAME_PALETTE       ((lv_palette_t) LV_PALETTE_NONE)
#define DEFAULT_DISPLAY_ORIENTATION     ((TunerOrientation) orientationNormal);
#define DEFAULT_EXP_SMOOTHING           ((float) 0.15)
#define DEFAULT_ONE_EU_BETA             ((float) 0.05)
#define DEFAULT_NOTE_DEBOUNCE_INTERVAL  ((float) 110.0)
#define DEFAULT_DISPLAY_BRIGHTNESS      ((float) 0.75)

enum TunerOrientation: uint8_t {
    orientationNormal,
    orientationLeft,
    orientationRight,
    orientationUpsideDown,
};

typedef void (*settings_changed_cb_t)();

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
    lv_display_t *lvglDisplay;

    nvs_handle_t    nvsHandle;

    settings_changed_cb_t settingsChangedCallback;

    /**
     * @brief Loads settings from persistent storage.
     */
    void loadSettings();

    void restoreDefaultSettings();
public:
    bool isShowingMenu = false;

    // User Setting Variables
    uint8_t             inTuneCentsWidth        = DEFAULT_IN_TUNE_CENTS_WIDTH;    // 2,                     Range 1 - 6
    lv_palette_t        noteNamePalette         = DEFAULT_NOTE_NAME_PALETTE; // white. Use lv_color_t c = lv_palette_main(LV_PALETTE_...) to get a color in LVGL. In the case of white, use lv_color_white()
    TunerOrientation    displayOrientation      = DEFAULT_DISPLAY_ORIENTATION;
    float               expSmoothing            = DEFAULT_EXP_SMOOTHING;   // 15 * .01 = 0.15,              Range: 0 - 100
    float               oneEUBeta               = DEFAULT_ONE_EU_BETA;   // 50 * .001 = 0.05,               Range: 0 - 1000
    float               noteDebounceInterval    = DEFAULT_NOTE_DEBOUNCE_INTERVAL;  // In milliseconds,      Range: 100 - 500
    float               displayBrightness       = DEFAULT_DISPLAY_BRIGHTNESS;   // 75 * .01 = 0.75,         Range: 10 - 100

    /**
     * @brief Create the settings object and sets its parameters
     * @param mainScreen The LVGL main screen in the app.
     */
    UserSettings(settings_changed_cb_t callback);

    /**
     * @brief Saves settings to persistent storage.
     */
    void saveSettings();

    /**
     * @brief Get the user setting for display orientation.
     */
    lv_display_rotation_t getDisplayOrientation();
    void setDisplayBrightness(float newBrightness);

    /**
     * @brief Gives UserSettings a handle to the main display and the main screen.
     * @param display Handle to the main display. Used for screen rotation.
     * @param screen Handle to the main screen. Used to close the settings menu.
     */
    void setDisplayAndScreen(lv_display_t *display, lv_obj_t *screen);

    /**
     * @brief Pause tuning or standby mode and show the settings menu/screen.
     */
    void showSettings();

    void createMenu(const char *buttonNames[], const char *buttonSymbols[], lv_palette_t *buttonColors, lv_event_cb_t eventCallbacks[], int numOfButtons);
    void removeCurrentMenu();
    void createSlider(const char *sliderName, int32_t minRange, int32_t maxRange, lv_event_cb_t sliderCallback, float *sliderValue);
    void createRoller(const char *title, const char *itemsString, lv_event_cb_t rollerCallback, uint8_t *rollerValue);
    void createSpinbox(const char *title, uint32_t minRange, uint32_t maxRange, uint32_t digitCount, uint32_t separatorPosition, float *spinboxValue, float conversionFactor);

    /**
     * @brief Exit the settings menu/screen and resume tuning/standby mode.
     */
    void exitSettings();

    void rotateScreenTo(TunerOrientation newRotation);
};

static void handleTunerButtonClicked(lv_event_t *e);
static void handleInTuneThresholdButtonClicked(lv_event_t *e);
static void handleInTuneThresholdButtonValueClicked(lv_event_t *e);
static void handleInTuneThresholdRoller(lv_event_t *e);

static void handleDisplayButtonClicked(lv_event_t *e);
static void handleBrightnessButtonClicked(lv_event_t *e);
static void handleBrightnessSlider(lv_event_t *e);

static void handleNoteColorButtonClicked(lv_event_t *e);
static void handleNoteColorSelected(lv_event_t *e);
static void handleNoteColorWhiteSelected(lv_event_t *e);
static void handleNoteColorRedSelected(lv_event_t *e);
static void handleNoteColorPinkSelected(lv_event_t *e);
static void handleNoteColorPurpleSelected(lv_event_t *e);
static void handleNoteColorBlueSelected(lv_event_t *e);
static void handleNoteColorGreenSelected(lv_event_t *e);
static void handleNoteColorOrangeSelected(lv_event_t *e);
static void handleNoteColorYellowSelected(lv_event_t *e);

static void handleIndicatorButtonClicked(lv_event_t *e);
static void handleRotationButtonClicked(lv_event_t *e);
static void handleRotationNormalClicked(lv_event_t *e);
static void handleRotationLeftClicked(lv_event_t *e);
static void handleRotationRightClicked(lv_event_t *e);
static void handleRotationUpsideDnClicked(lv_event_t *e);

static void handleDebugButtonClicked(lv_event_t *e);
static void handleExpSmoothingButtonClicked(lv_event_t *e);
static void handle1EUBetaButtonClicked(lv_event_t *e);
static void handle1EUFilterFirstButtonClicked(lv_event_t *e);
static void handleMovingAvgButtonClicked(lv_event_t *e);
static void handleNameDebouncingButtonClicked(lv_event_t *e);
static void handleAboutButtonClicked(lv_event_t *e);
static void handleBackButtonClicked(lv_event_t *e);
static void handleExitButtonClicked(lv_event_t *e);
