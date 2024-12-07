/*=============================================================================
    UserSettings.cpp
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "UserSettings.h"

#define MENU_BTN_TUNER              "Tuner"
#define MENU_BTN_IN_TUNE_THRESHOLD  "In-Tune Threshold"

#define MENU_BTN_DISPLAY            "Display"
    #define MENU_BTN_BRIGHTNESS         "Brightness"
    #define MENU_BTN_NOTE_COLOR         "Note Color"
    #define MENU_BTN_ROTATION           "Rotation"
        #define MENU_BTN_ROTATION_NORMAL    "Normal"
        #define MENU_BTN_ROTATION_LEFT      "Left"
        #define MENU_BTN_ROTATION_RIGHT     "Right"
        #define MENU_BTN_ROTATION_UPSIDE_DN "Upside Down"

#define MENU_BTN_DEBUG              "Advanced"
#define MENU_BTN_EXP_SMOOTHING      "Exp Smoothing"
#define MENU_BTN_1EU_BETA           "1 EU Beta"
#define MENU_BTN_1EU_FLTR_1ST       "1 EU 1st?"
#define MENU_BTN_MOVING_AVG         "Moving Average"
#define MENU_BTN_NAME_DEBOUNCING    "Name Debouncing"

#define MENU_BTN_ABOUT              "About"

#define MENU_BTN_BACK               "Back"
#define MENU_BTN_EXIT               "Exit"

// Setting keys in NVS can only be up to 15 chars max
#define SETTING_KEY_IN_TUNE_WIDTH           "in_tune_width"
#define SETTING_KEY_NOTE_NAME_PALETTE       "note_nm_palette"
#define SETTING_KEY_DISPLAY_ORIENTATION     "display_orient"
#define SETTING_KEY_EXP_SMOOTHING           "exp_smoothing"
#define SETTING_KEY_ONE_EU_BETA             "one_eu_beta"
#define SETTING_KEY_NOTE_DEBOUNCE_INTERVAL  "note_debounce"
#define SETTING_KEY_DISPLAY_BRIGHTNESS      "disp_brightness"

/*

SETTINGS
    Tuning
        [X] In Tune Width
            Show a slider and allow the user to choose integer values between 1 and 10. Save the setting into "user_in_tune_cents_width"
        [x] Back - returns to the main menu

    Display Settings
        [x] Brightness
            Show a slider and show values between 10 and 100. Save to a setting named "user_display_brightness" as a float value between 0.1 and 1.0
        [ ] Note Color
            Show a color picker and save setting to a variable named "user_note_name_color"
        [x] Rotation
            Allow the user to choose between: Normal, Upside Down, Left, or Right. Save the setting into "user_rotation_mode"
        [x] Back - returns to the main menu

    Debug
        [ ] Exp Smoothing
            Allow the user to use a slider to choose a float value between 0.0 and 1.0. Show only 1 decimal after the decimal point. Save the setting into "user_exp_smoothing"
        [ ] 1EU Beta
            Allow the user to use a slider to choose a float value between 0.000 and 2.000 allowing up to 3 decimal places of granularity. Save the setting into "user_1eu_beta"
        [ ] Note Debouncing
            Allow the user to use a slider to choose an integer value between 100 and 400. Save this setting into "user_note_debounce_interval"
        [x] Back - returns to the main menu

    About
        [ ] Show version information
        [ ] Acknowledgements
        [ ] Restore Factory Defaults
            [ ] Confirmation Yes/No
        [ ] Back
        
    Exit -> Selecting this menu exits the settings menu
*/



//
// PRIVATE Methods
//

void UserSettings::loadSettings() {
    nvs_flash_init();
    nvs_open("settings", NVS_READWRITE, &nvsHandle);

    uint8_t value;

    if (nvs_get_u8(nvsHandle, SETTING_KEY_IN_TUNE_WIDTH, &value) == ESP_OK) {
        inTuneCentsWidth = value;
    } else {
        inTuneCentsWidth = DEFAULT_IN_TUNE_CENTS_WIDTH;
    }

    if (nvs_get_u8(nvsHandle, SETTING_KEY_NOTE_NAME_PALETTE, &value) == ESP_OK) {
        noteNamePalette = (lv_palette_t)value;
    } else {
        noteNamePalette = DEFAULT_NOTE_NAME_PALETTE;
    }

    if (nvs_get_u8(nvsHandle, SETTING_KEY_DISPLAY_ORIENTATION, &value) == ESP_OK) {
        displayOrientation = (TunerOrientation)value;
    } else {
        displayOrientation = DEFAULT_DISPLAY_ORIENTATION;
    }
    ESP_LOGI("Settings", "Loaded display orientation of: %d", displayOrientation);

    if (nvs_get_u8(nvsHandle, SETTING_KEY_EXP_SMOOTHING, &value) == ESP_OK) {
        expSmoothing = ((float)value) * 0.01;
    } else {
        expSmoothing = DEFAULT_EXP_SMOOTHING;
    }

    if (nvs_get_u8(nvsHandle, SETTING_KEY_ONE_EU_BETA, &value) == ESP_OK) {
        oneEUBeta = ((float)value) * 0.001;
    } else {
        oneEUBeta = DEFAULT_ONE_EU_BETA;
    }

    if (nvs_get_u8(nvsHandle, SETTING_KEY_NOTE_DEBOUNCE_INTERVAL, &value) == ESP_OK) {
        noteDebounceInterval = value;
    } else {
        noteDebounceInterval = DEFAULT_NOTE_DEBOUNCE_INTERVAL;
    }

    if (nvs_get_u8(nvsHandle, SETTING_KEY_DISPLAY_BRIGHTNESS, &value) == ESP_OK) {
        displayBrightness = ((float)value) * 0.01;
    } else {
        displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;
    }
}

void UserSettings::restoreDefaultSettings() {
    inTuneCentsWidth = DEFAULT_IN_TUNE_CENTS_WIDTH;
    noteNamePalette = DEFAULT_NOTE_NAME_PALETTE;
    displayOrientation = DEFAULT_DISPLAY_ORIENTATION;
    expSmoothing = DEFAULT_EXP_SMOOTHING;
    oneEUBeta = DEFAULT_ONE_EU_BETA;
    noteDebounceInterval = DEFAULT_NOTE_DEBOUNCE_INTERVAL;
    displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;

    saveSettings();

    // Reboot!
    esp_restart();
}

void UserSettings::showTunerMenu() {

}

void UserSettings::showDisplayMenu() {
    // if (!lvgl_port_lock(0)) {
    //     return;
    // }
    // ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_90));
    // lvgl_port_unlock();
}

void UserSettings::showAdvancedMenu() {

}

void UserSettings::showAboutMenu() {

}

//
// PUBLIC Methods
//

UserSettings::UserSettings() {
    loadSettings();
}

void UserSettings::saveSettings() {
    uint8_t value;

    value = inTuneCentsWidth;
    nvs_set_u8(nvsHandle, SETTING_KEY_IN_TUNE_WIDTH, value);

    value = (uint8_t)noteNamePalette;
    nvs_set_u8(nvsHandle, SETTING_KEY_NOTE_NAME_PALETTE, value);

    value = (uint8_t)displayOrientation;
    nvs_set_u8(nvsHandle, SETTING_KEY_DISPLAY_ORIENTATION, value);

    value = (uint8_t)(expSmoothing * 100);
    nvs_set_u8(nvsHandle, SETTING_KEY_EXP_SMOOTHING, value);

    value = (uint8_t)(oneEUBeta * 1000);
    nvs_set_u8(nvsHandle, SETTING_KEY_ONE_EU_BETA, value);

    value = (uint8_t)noteDebounceInterval;
    nvs_set_u8(nvsHandle, SETTING_KEY_NOTE_DEBOUNCE_INTERVAL, value);

    value = (uint8_t)(displayBrightness * 100);
    nvs_set_u8(nvsHandle, SETTING_KEY_DISPLAY_BRIGHTNESS, value);

    nvs_commit(nvsHandle);
}

lv_display_rotation_t UserSettings::getDisplayOrientation() {
    switch (this->displayOrientation) {
        case orientationNormal:
            return LV_DISPLAY_ROTATION_180;
        case orientationLeft:
            return LV_DISPLAY_ROTATION_90;
        case orientationRight:
            return LV_DISPLAY_ROTATION_270;
        default:
            return LV_DISPLAY_ROTATION_0;
    }
}

void UserSettings::setDisplayAndScreen(lv_display_t *display, lv_obj_t *screen) {
    lvglDisplay = display;
    screenStack.push_back(screen);
}

void UserSettings::showSettings() {
    isShowingMenu = true;
    const char *buttonNames[] = {
        MENU_BTN_TUNER,
        MENU_BTN_DISPLAY,
        MENU_BTN_DEBUG,
        MENU_BTN_ABOUT,
        // MENU_BTN_EXIT,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleTunerButtonClicked,
        handleDisplayButtonClicked,
        handleDebugButtonClicked,
        handleAboutButtonClicked,
    };
    createMenu(buttonNames, callbackFunctions, 4);
}

void UserSettings::createMenu(const char *buttonNames[], lv_event_cb_t eventCallbacks[], int numOfButtons) {
    // Create a new screen, add all the buttons to it,
    // add the screen to the stack, and activate the new screen
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_obj_t *scr = lv_obj_create(NULL);

    // Create a scrollable container
    lv_obj_t *scrollable = lv_obj_create(scr);
    lv_obj_set_size(scrollable, lv_pct(100), lv_pct(100)); // Full size of the parent
    lv_obj_set_flex_flow(scrollable, LV_FLEX_FLOW_COLUMN); // Arrange children in a vertical list
    lv_obj_set_scroll_dir(scrollable, LV_DIR_VER);         // Enable vertical scrolling
    lv_obj_set_scrollbar_mode(scrollable, LV_SCROLLBAR_MODE_AUTO); // Show scrollbar when scrolling
    lv_obj_set_style_pad_all(scrollable, 10, 0);           // Add padding for aesthetics
    lv_obj_set_style_bg_color(scrollable, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), 0); // Optional background color

    lv_obj_t *btn;
    lv_obj_t *label;

    int32_t buttonWidthPercentage = 100;

    for (int i = 0; i < numOfButtons; i++) {
        ESP_LOGI("Settings", "Creating menu item: %d of %d", i, numOfButtons);
        const char *buttonName = buttonNames[i];
        lv_event_cb_t eventCallback = eventCallbacks[i];
        btn = lv_btn_create(scrollable);
        lv_obj_set_width(btn, lv_pct(buttonWidthPercentage));
        lv_obj_set_user_data(btn, this);
        lv_obj_add_event_cb(btn, eventCallback, LV_EVENT_CLICKED, btn);
        label = lv_label_create(btn);
        lv_label_set_text_static(label, buttonName);
    }

    if (screenStack.size() == 1) {
        // We're on the top menu - include an exit button
        btn = lv_btn_create(scrollable);
        lv_obj_set_user_data(btn, this);
        lv_obj_set_width(btn, lv_pct(buttonWidthPercentage));
        lv_obj_add_event_cb(btn, handleExitButtonClicked, LV_EVENT_CLICKED, btn);
        label = lv_label_create(btn);
        lv_label_set_text_static(label, MENU_BTN_EXIT);
    } else {
        // We're in a submenu - include a back button
        btn = lv_btn_create(scrollable);
        lv_obj_set_user_data(btn, this);
        lv_obj_set_width(btn, lv_pct(buttonWidthPercentage));
        lv_obj_add_event_cb(btn, handleBackButtonClicked, LV_EVENT_CLICKED, btn);
        label = lv_label_create(btn);
        lv_label_set_text_static(label, MENU_BTN_BACK);
    }

    screenStack.push_back(scr); // Save the new screen on the stack
    lv_screen_load(scr);        // Activate the new screen
    lvgl_port_unlock();
}

void UserSettings::removeCurrentMenu() {
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_obj_t *currentScreen = screenStack.back();
    screenStack.pop_back();

    lv_obj_t *parentScreen = screenStack.back();
    lv_scr_load(parentScreen);      // Show the parent screen

    lv_obj_clean(currentScreen);    // Clean up the screen so memory is cleared from sub items
    lv_obj_del(currentScreen);      // Remove the old screen from memory

    lvgl_port_unlock();
}

void UserSettings::createSlider(const char *sliderName, int32_t minRange, int32_t maxRange, lv_event_cb_t sliderCallback, float *sliderValue) {
    // Create a new screen, add the slider to it, add the screen to the stack,
    // and activate the new screen.
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_obj_t *scr = lv_obj_create(NULL);

    // Create a scrollable container
    lv_obj_t *scrollable = lv_obj_create(scr);
    lv_obj_remove_style_all(scrollable);
    lv_obj_set_size(scrollable, lv_pct(100), lv_pct(100)); // Full size of the parent
    lv_obj_set_flex_flow(scrollable, LV_FLEX_FLOW_COLUMN); // Arrange children in a vertical list
    lv_obj_set_scroll_dir(scrollable, LV_DIR_VER);         // Enable vertical scrolling
    lv_obj_set_scrollbar_mode(scrollable, LV_SCROLLBAR_MODE_AUTO); // Show scrollbar when scrolling
    lv_obj_set_style_pad_all(scrollable, 10, 0);           // Add padding for aesthetics
    lv_obj_set_style_bg_color(scrollable, lv_color_black(), 0); // Optional background color

    // Show the title of the screen at the top middle
    lv_obj_t *label = lv_label_create(scrollable);
    lv_label_set_text_static(label, sliderName);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *spacer = lv_obj_create(scrollable);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, lv_pct(100));
    lv_obj_set_flex_grow(spacer, 2);

    // Create a slider in the center of the display
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_center(slider);
    lv_obj_set_user_data(slider, this); // Send "this" as the user data of the slider
    lv_slider_set_value(slider, *sliderValue * 100, LV_ANIM_OFF);
    lv_obj_center(slider);
    lv_obj_add_event_cb(slider, sliderCallback, LV_EVENT_VALUE_CHANGED, sliderValue); // Send the slider value as the event user data
    lv_obj_set_style_anim_duration(slider, 2000, 0);

    // We're in a submenu - include a back button
    lv_obj_t *btn = lv_btn_create(scrollable);
    lv_obj_set_user_data(btn, this);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_add_event_cb(btn, handleBackButtonClicked, LV_EVENT_CLICKED, btn);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, MENU_BTN_BACK);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    screenStack.push_back(scr); // Save the new screen on the stack
    lv_screen_load(scr);        // Activate the new screen
    lvgl_port_unlock();
}

void UserSettings::createRoller(const char *title, const char *itemsString, lv_event_cb_t rollerCallback, uint8_t *rollerValue) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    lv_obj_t *scr = lv_obj_create(NULL);

    // Create a scrollable container
    lv_obj_t *scrollable = lv_obj_create(scr);
    lv_obj_set_size(scrollable, lv_pct(100), lv_pct(100)); // Full size of the parent
    lv_obj_set_flex_flow(scrollable, LV_FLEX_FLOW_COLUMN); // Arrange children in a vertical list
    lv_obj_set_scroll_dir(scrollable, LV_DIR_VER);         // Enable vertical scrolling
    lv_obj_set_scrollbar_mode(scrollable, LV_SCROLLBAR_MODE_AUTO); // Show scrollbar when scrolling
    lv_obj_set_style_pad_all(scrollable, 10, 0);           // Add padding for aesthetics
    lv_obj_set_style_bg_color(scrollable, lv_color_black(), 0); // Optional background color

    // Show the title of the screen at the top middle
    lv_obj_t *label = lv_label_create(scrollable);
    lv_label_set_text_static(label, title);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    // lv_obj_t *spacer = lv_obj_create(scrollable);
    // lv_obj_remove_style_all(spacer);
    // lv_obj_set_width(spacer, lv_pct(100));
    // lv_obj_set_flex_grow(spacer, 2);

    lv_obj_t * roller = lv_roller_create(scrollable);
    lv_obj_set_user_data(roller, this);
    lv_roller_set_options(roller, itemsString, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 4);
    lv_roller_set_selected(roller, *rollerValue - 1, LV_ANIM_OFF); // TODO: This "-1" is for the In-Tune Threshold and this function should be made to be more generic
    lv_obj_set_width(roller, lv_pct(100));
    lv_obj_set_flex_grow(roller, 2);
    lv_obj_add_event_cb(roller, rollerCallback, LV_EVENT_ALL, rollerValue);


    lv_obj_t *btn = lv_btn_create(scrollable);
    lv_obj_set_user_data(btn, this);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_add_event_cb(btn, handleBackButtonClicked, LV_EVENT_CLICKED, btn);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, MENU_BTN_BACK);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    screenStack.push_back(scr); // Save the new screen on the stack
    lv_screen_load(scr);        // Activate the new screen
    lvgl_port_unlock();
}

void UserSettings::exitSettings() {
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_obj_t *mainScreen = screenStack.front();
    lv_scr_load(mainScreen);

    // Remove all but the first item out of the screenStack.
    while (screenStack.size() > 1) {
        lv_obj_t *scr = screenStack.back();
        lv_obj_clean(scr);  // Clean up sub object memory
        lv_obj_del(scr);

        screenStack.pop_back();
    }

    lvgl_port_unlock();
    isShowingMenu = false;
}

void UserSettings::rotateScreenTo(TunerOrientation newRotation) {
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_display_rotation_t new_rotation = LV_DISPLAY_ROTATION_0;
    switch (newRotation)
    {
    case orientationNormal:
        new_rotation = LV_DISPLAY_ROTATION_180;
        break;
    case orientationLeft:
        new_rotation = LV_DISPLAY_ROTATION_90;
        break;
    case orientationRight:
        new_rotation = LV_DISPLAY_ROTATION_270;
        break;
    default:
        new_rotation = LV_DISPLAY_ROTATION_0;
        break;
    }

    if (lv_display_get_rotation(lvglDisplay) != new_rotation) {
        ESP_ERROR_CHECK(lcd_display_rotate(lvglDisplay, new_rotation));

        // Save this off into user preferences.
        this->displayOrientation = newRotation;
        this->saveSettings();
    }

    lvgl_port_unlock();
}

static void handleExitButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Exit button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->exitSettings();
}

static void handleTunerButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Tuner button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    const char *buttonNames[] = {
        MENU_BTN_IN_TUNE_THRESHOLD,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleInTuneThresholdButtonClicked,
    };
    settings->createMenu(buttonNames, callbackFunctions, 1);
}

static void handleInTuneThresholdButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "In Tune Threshold button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->createRoller((const char *)MENU_BTN_IN_TUNE_THRESHOLD,
                           (const char *)"+/- 1 cent\n"
                           "+/- 2 cents\n"
                           "+/- 3 cents\n"
                           "+/- 4 cents\n"
                           "+/- 5 cents\n"
                           "+/- 6 cents",
                           handleInTuneThresholdRoller,
                           &settings->inTuneCentsWidth);
}

static void handleInTuneThresholdRoller(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t *rollerValue = (uint8_t *)lv_event_get_user_data(e);

    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        uint32_t selectedIndex = lv_roller_get_selected(roller);
        ESP_LOGI("Settings", "In Tune Threshold Roller index selected: %ld", selectedIndex);
        *rollerValue = selectedIndex + 1; // TODO: Make this work for other things too
    }

    lvgl_port_unlock();
}

static void handleInTuneThresholdButtonValueClicked(lv_event_t *e) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    ESP_LOGI("Settings", "In Tune Threshold value clicked");
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label == NULL) {
        ESP_LOGI("SETTINGS", "Label is null");
        lvgl_port_unlock();
        return;
    }
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data(btn);
    char *text = lv_label_get_text(label);
    if (strcmp(text, "1") == 0) {
        settings->inTuneCentsWidth = 1;
    } else if (strcmp(text, "2") == 0) {
        settings->inTuneCentsWidth = 2;
    } else if (strcmp(text, "3") == 0) {
        settings->inTuneCentsWidth = 3;
    } else if (strcmp(text, "4") == 0) {
        settings->inTuneCentsWidth = 4;
    } else if (strcmp(text, "5") == 0) {
        settings->inTuneCentsWidth = 5;
    } else if (strcmp(text, "6") == 0) {
        settings->inTuneCentsWidth = 6;
    } else if (strcmp(text, "7") == 0) {
        settings->inTuneCentsWidth = 7;
    } else if (strcmp(text, "8") == 0) {
        settings->inTuneCentsWidth = 8;
    }
    lvgl_port_unlock();
}

static void handleDisplayButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Display button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    const char *buttonNames[] = {
        MENU_BTN_BRIGHTNESS,
        MENU_BTN_NOTE_COLOR,
        MENU_BTN_ROTATION,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleBrightnessButtonClicked,
        handleNoteColorButtonClicked,
        handleRotationButtonClicked,
    };
    settings->createMenu(buttonNames, callbackFunctions, 3);
}

static void handleBrightnessButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Brightness slider changed");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->createSlider(MENU_BTN_BRIGHTNESS, 10, 100, handleBrightnessSlider, &settings->displayBrightness);
}

static void handleBrightnessSlider(lv_event_t *e) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    float *sliderValue = (float *)lv_event_get_user_data(e);
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data(slider);

    uint8_t newValue = (uint8_t)lv_slider_get_value(slider);

    if (lcd_display_brightness_set(newValue) == ESP_OK) {
        *sliderValue = (float)newValue * 0.01;
        ESP_LOGI("Settings", "New slider value: %.2f", *sliderValue);
    }
    lvgl_port_unlock();
}

static void handleNoteColorButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
}

static void handleRotationButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    const char *buttonNames[] = {
        MENU_BTN_ROTATION_NORMAL,
        MENU_BTN_ROTATION_LEFT,
        MENU_BTN_ROTATION_RIGHT,
        MENU_BTN_ROTATION_UPSIDE_DN,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleRotationNormalClicked,
        handleRotationLeftClicked,
        handleRotationRightClicked,
        handleRotationUpsideDnClicked,
    };
    settings->createMenu(buttonNames, callbackFunctions, 4);
}

static void handleRotationNormalClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Normal clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->rotateScreenTo(orientationNormal);
}

static void handleRotationLeftClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Left clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->rotateScreenTo(orientationLeft);
}

static void handleRotationRightClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Right clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->rotateScreenTo(orientationRight);
}

static void handleRotationUpsideDnClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Upside Down clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->rotateScreenTo(orientationUpsideDown);
}

static void handleDebugButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handleExpSmoothingButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handle1EUBetaButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handle1EUFilterFirstButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handleMovingAvgButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handleNameDebouncingButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handleAboutButtonClicked(lv_event_t *e) {
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();

}

static void handleBackButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Back button clicked");
    UserSettings *settings;
    if (!lvgl_port_lock(0)) {
        return;
    }
    settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    lvgl_port_unlock();
    settings->saveSettings(); // TODO: Figure out a better way of doing this than saving every time
    settings->removeCurrentMenu();
}

