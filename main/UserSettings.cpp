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

UserSettings::UserSettings(lv_obj_t * mainScreen) {
    screenStack.push_back(mainScreen); // Add the main screen so we can load this when exiting the menu
}

void UserSettings::showSettings() {
    if (!lvgl_port_lock(0)) {
        return;
    }
    // Create a new screen for the main menu
    lv_obj_t *scr = lv_obj_create(NULL);

    // Create a scrollable container
    lv_obj_t *scrollable = lv_obj_create(scr);
    lv_obj_set_size(scrollable, lv_pct(100), lv_pct(100)); // Full size of the parent
    lv_obj_set_flex_flow(scrollable, LV_FLEX_FLOW_COLUMN); // Arrange children in a vertical list
    lv_obj_set_scroll_dir(scrollable, LV_DIR_VER);         // Enable vertical scrolling
    lv_obj_set_scrollbar_mode(scrollable, LV_SCROLLBAR_MODE_AUTO); // Show scrollbar when scrolling
    lv_obj_set_style_pad_all(scrollable, 10, 0);           // Add padding for aesthetics
    lv_obj_set_style_bg_color(scrollable, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), 0); // Optional background color
    // lv_obj_set_style_bg_color(scrollable, lv_palette_lighten(LV_PALETTE_GREY, 4), 0); // Optional background color
    // lv_obj_set_style_bg_color(scrollable, lv_color_black(), 0); // Optional background color

    lv_obj_t *btn;
    lv_obj_t *label;

    // Tuning Button
    btn = lv_btn_create(scrollable);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_user_data(btn, (void *)MENU_BTN_TUNER);
    // lv_obj_add_event_cb(btn, &UserSettings::handleButtonClick, LV_EVENT_CLICKED, btn);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, MENU_BTN_TUNER);

    // Exit Button
    btn = lv_btn_create(scrollable);
    lv_obj_set_user_data(btn, this);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_add_event_cb(btn, handleExitButtonClicked, LV_EVENT_CLICKED, btn);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, MENU_BTN_EXIT);

    screenStack.push_back(scr);

    lv_screen_load(scr);
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
        lv_obj_del(scr);

        screenStack.pop_back();
    }

    lvgl_port_unlock();
}

static void handleExitButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    settings->exitSettings();
}

static void handleTunerButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    ESP_LOGI("Settings", "Tuner button clicked");
}

static void handleInTuneThresholdButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

static void handleDisplayButtonClicked(lv_event_t *e) {
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

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
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

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
    UserSettings *settings = (UserSettings *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));

}

