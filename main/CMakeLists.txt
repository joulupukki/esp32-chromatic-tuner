# ESP32 Heltec
# idf_component_register(SRCS "tuner_font_images.c" "tuner_ui_strobe.cpp" "tuner_ui_needle.cpp" "tuner_gui_task.cpp" "pitch_detector.cpp" "globals.cpp" "fontawesome_48.c" "UserSettings.cpp" "raleway_128.c" "OneEuroFilter.cpp" "main.cpp"
#                     INCLUDE_DIRS "."
#                     REQUIRES esp_lcd driver esp_adc
#                     )

# ESP32-CYD
idf_component_register(SRCS "lcd.c" "touch.c" "OneEuroFilter.cpp" "raleway_128.c" "tuner_font_images.c" "globals.cpp" "UserSettings.cpp" "fontawesome_48.c" "pitch_detector_task.cpp" "tuner_gui_task.cpp" "tuner_ui_needle.cpp" "tuner_ui_strobe.cpp" "main.cpp"
                    INCLUDE_DIRS "."
#                    REQUIRES esp_lcd driver esp_adc hal
                    REQUIRES esp_lcd driver esp_adc nvs_flash
                    )

target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/extra_components/q/q_lib/include ${CMAKE_SOURCE_DIR}/extra_components/q/infra/include)