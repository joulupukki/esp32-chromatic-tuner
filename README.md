# Q-Tune Chromatic Tuner
This is an in-progress project with the goal of creating a fully open-source chromatic tuner in a standard pedal enclosure for use on pedalboards and based on the ESP32 MCU.

The project uses the Q DSP Library for frequency detection and LVGL for the
graphical user interface.

The ESP32 board is a ESP32-2432S028R (ESP32-WROOM-32: https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf), more commonly known as ESP32 CYD (Cheap Yellow Display). For more information, take a look at this GitHub repo: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

If you are interested in building this tuner, be aware that there are a few variants of the ESP32 CYD available. Make sure you purchase one that only has one USB port, a single Micro USB.

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
8. Set your ESP32 target (esp32)
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Set Espressif Device Target`
    - Select `esp32`
    - Select the `ESP32 chip (via ESP-PROG)` option
9. Select the port to use
    - Plug in your ESP32 dev board and wait for a few seconds
    - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Select Port to Use (COM, tty, usbserial)`
    - Select your port from the drop-down list that appears
    - Windows Only: If you receive an error while trying to locate the serial/COM port that your ESP32 is connected to (or it doesn't show up), you may need to install the driver for the CH340C UART chip that the ESP32-CYD uses for USB communication. The driver is available from the manufacturer here: https://www.wch-ic.com/downloads/CH341SER_ZIP.html
        - After installing the driver and connecting the CYD, re-run the "Select Port" command
10. Attempt to build the software
    - Use the Command Palette and select `ESP-IDF: Build your Project`
11. Assuming step #9 worked, Build and install the software
    - Use the Command Palette and select `ESP-IDF: Build, Flash, and Start a Monitor on your Device`
    - To stop monitoring the output, press Control+T, then X

## Demo

Here's a simple demo of how the project is coming along as of 10 Dec 2024:
[![Demo Video](https://img.youtube.com/vi/im-Qe8d9LSk/0.jpg)](https://youtu.be/im-Qe8d9LSk)

Here's an older demo, more of a proof-of-concept on a smaller screen using a Heltec ESP32-S3 (20 Aug 2024):

[![Demo Video](https://img.youtube.com/vi/XWTicIlTI_k/0.jpg)](https://youtu.be/XWTicIlTI_k)

## License

This software is licensed under the GNU General Public License (GPL) for open-source use. A commercial license is available for those who wish to use this software in proprietary or closed-source projects. For more details, please contact btimothy [at] gmail [dot] com.