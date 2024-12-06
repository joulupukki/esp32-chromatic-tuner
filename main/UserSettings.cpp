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
    #define MENU_BTN_INDICATOR_COLOR    "Indicator Color"
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

/*

SETTINGS
    Tuning
        In Tune Width
            Show a slider and allow the user to choose integer values between 1 and 10. Save the setting into "user_in_tune_cents_width"
        Back - returns to the main menu

    Display Settings
        Brightness
            Show a slider and show values between 10 and 100. Save to a setting named "user_display_brightness" as a float value between 0.1 and 1.0
        Note Color
            Show a color picker and save setting to a variable named "user_note_name_color"
        Indicator Color
            Show a color picker and save the estting into a variable named "user_pitch_indicator_color"
        Indicator Width
            Show a slider with integer values between 4 and 20. Save the setting into "user_pitch_indicator_width"
        Rotation
            Allow the user to choose between: Normal, Upside Down, Left, or Right. Save the setting into "user_rotation_mode"
        Back - returns to the main menu

    Debug
        Exp Smoothing
            Allow the user to use a slider to choose a float value between 0.0 and 1.0. Show only 1 decimal after the decimal point. Save the setting into "user_exp_smoothing"
        1EU Beta
            Allow the user to use a slider to choose a float value between 0.000 and 2.000 allowing up to 3 decimal places of granularity. Save the setting into "user_1eu_beta"
        Note Debouncing
            Allow the user to use a slider to choose an integer value between 100 and 400. Save this setting into "user_note_debounce_interval"
        Back - returns to the main menu

    About
        Show version information
        
    Save & Exit -> Selecting this menu exits the settings menu
*/



//
// PRIVATE Methods
//
void UserSettings::saveSettings() {

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

UserSettings::UserSettings(lv_display_t *display, lv_obj_t * mainScreen) {
    lvglDisplay = display;
    screenStack.push_back(mainScreen); // Add the main screen so we can load this when exiting the menu
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

        // TODO: Save this off into user preferences.
    }

    lvgl_port_unlock();
}

static void handleExitButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Exit button clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->exitSettings();
}

static void handleTunerButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Tuner button clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    const char *buttonNames[] = {
        MENU_BTN_IN_TUNE_THRESHOLD,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleInTuneThresholdButtonClicked,
    };
    settings->createMenu(buttonNames, callbackFunctions, 1);
}

static void handleInTuneThresholdButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleDisplayButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Display button clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    const char *buttonNames[] = {
        MENU_BTN_BRIGHTNESS,
        MENU_BTN_NOTE_COLOR,
        MENU_BTN_INDICATOR_COLOR,
        MENU_BTN_ROTATION,
    };
    lv_event_cb_t callbackFunctions[] = {
        handleBrightnessButtonClicked,
        handleNoteColorButtonClicked,
        handleIndicatorButtonClicked,
        handleRotationButtonClicked,
    };
    settings->createMenu(buttonNames, callbackFunctions, 4);
}

static void handleBrightnessButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleNoteColorButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleIndicatorButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleRotationButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation button clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
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
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->rotateScreenTo(orientationNormal);
}

static void handleRotationLeftClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Left clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->rotateScreenTo(orientationLeft);
}

static void handleRotationRightClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Right clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->rotateScreenTo(orientationRight);
}

static void handleRotationUpsideDnClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Rotation Upside Down clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->rotateScreenTo(orientationUpsideDown);
}

static void handleDebugButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleExpSmoothingButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handle1EUBetaButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handle1EUFilterFirstButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleMovingAvgButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleNameDebouncingButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleAboutButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleBackButtonClicked(lv_event_t *e) {
    ESP_LOGI("Settings", "Back button clicked");
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->removeCurrentMenu();
}

