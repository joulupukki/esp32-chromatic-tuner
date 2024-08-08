## Project Overview
This is an in-progress project to build an open source DIY guitar pedal tuner using an ESP32 S3 Dev Board. The project uses Q DSP Library for frequency detection and uses LVGL for displaying the results on the built-in OLED.

Dev board is the Heltec WiFi Kit 32 (V3): https://heltec.org/project/wifi-kit32-v3/

## Setup

1. Install Visual Studio Code
2. Install the Visual Studio Code ESP-IDF extension
3. Configure the ESP-IDF extension by using the express or advanced mode
    - Be sure to choose **v5.3** as the version of ESP-IDF to use
    - You may need to use advanced mode to be able to select the correct location for python3
    - If pip error, navigate to: C:\Espressif\tools\idf-python\3.11.2> and run: python -m ensurepip --upgrade
4. Make sure you have `git` installed
5. Clone this github project
6. From a terminal go into the project and do this:
    ```
    git submodule update --init --recursive
    ```
7. Open the esp32-chromatic-tuner folder in VS Code
8. Set your ESP32 target (esp32s3)
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Set Espressif Device Target`
    - Select `esp32s3`
    - Select the `via ESP PROG` option
9. Select the port to use
    - Plug in your ESP32 dev board and wait for a few seconds
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Select Port to Use (COM, tty, usbserial)`
    - Select your port from the drop-down list that appears
10. Attempt to build the software
    - Use the Command Palette and select `ESP-IDF: Build your Project`
11. Assuming step #9 worked, Build and install the software
    - Use the Command Palette and select `ESP-IDF: Build, Flash, and Start a Monitor on your Device`
    - To stop monitoring the output, press Control+T, then X
