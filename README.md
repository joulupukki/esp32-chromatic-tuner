## Project Overview
This is an in-progress project to build an open source DIY guitar pedal tuner using an ESP32 S3 Dev Board. The project uses Q DSP Library for frequency detection and uses LVGL for displaying the results on the built-in OLED.

Dev board is the Heltec WiFi Kit 32 (V3): https://heltec.org/project/wifi-kit32-v3/

## Setup

1. Install Visual Studio Code
2. Install the ESP-IDF v5.3 extension
3. Make sure you have `git` installed
4. Clone this github project
5. From a terminal go into the project and do this:
    ```
    git submodule update --init --recursive
    ```
6. Modify `extra_/_components/q_lib/CMakeLists.txt` and `extra_components/infra/CMakeLists.txt`
7. Set your ESP32 target (esp32s3)
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Set Espressif Device Target`
    - Select `esp32s3`
    - Select the `via ESP PROG` option
8. Select the port to use
    - Plug in your ESP32 dev board and wait for a few seconds
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Select Port to Use (COM, tty, usbserial)`
    - Select your port from the drop-down list that appears
9. Attempt to build the software
    - Use the Command Palette and select `ESP-IDF: Build your Project`
10. Assuming step #9 worked, Build and install the software
    - Use the Command Palette and select `ESP-IDF: Build, Flash, and Start a Monitor on your Device`