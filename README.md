This is not yet functional but the plan is to have a working chromatic tuner based on the Heltec WiFi Kit 32 (V3): https://heltec.org/project/wifi-kit32-v3/

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


## EVERYTHING BELOW HERE IS OLD

9. Install `lvgl/lvgl`
    - Open the Command Palette (Command+Shift+P) and select `ESP-IDF: Welcome`
    - Click the `Components manager` button
    - Search for `lvgl`
    - Select v8.4.0 from the version dropdown
    - Click the `Install` button
10. Install `espressif/esp_lvgl_port`
    - Use the command palette similar to what you did in step #9
    - Select v2.2.2


TODO:
9. Add LVGL with the VS Code ESP Components tool (ma)
10. Add esp_lvgl_port with the VS Code ESP Components tool
// Add lvgl_esp32_drivers with the VS Code ESP Components tool
// 6. Go to the root of the project and add the lgvl_esp32_drivers as a submodule: `git submodule add https://github.com/lvgl/lvgl_esp32_drivers.git components/lvgl_esp32_drivers`
TODO: Fix these instructions: 8. Go to the root of the project and clone the Q DSP Library: `git submodule ad https:....(TODO)


# Do not include a project name if building for ESP-IDF

Goal:
Build an ESP-IDF app that runs on ESP32v3, uses q_lib for frequency detection, and uses the built-in OLED to display tuning info.

Plan of attack:
1. Get the project set up with Visual Studio Code and the ESP-IDF extension
2. Add q_lib as a submodule
3. Get the project to build without errors
4. Figure out how to do continuous analog signal reading using ADC continuous read
5. Figure out how to feed the data into q_lib for frequency detection
6. Figure out how to pull in the Heltec OLED library into the project as a component
6. Figure out how to use the OLED

The rest of this README is just the sample info that came from the ESP-IDF adc_continuous_read sample project.

----

| Supported Targets | ESP32 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# ADC DMA Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example shows how to use ADC Continuous Read Mode (DMA Mode) to read from GPIO pins via on-chip ADC module(s).

## How to use example

### Hardware Required

* A development board with ESP32, ESP32-S and ESP32-C SoC
* A USB cable for power supply and programming

Please refer to the `channel` array defined in `adc_dma_example_main.c`. These GPIOs should be connected to voltage sources (0 ~ 3.3V). If other ADC unit(s) / channel(s) are selected in your application,
feel free to modify these definitions.

### Configure the project

```
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Running this example, you will see the following log output on the serial monitor:
```
I (367) EXAMPLE: adc_pattern[0].atten is :0
I (377) EXAMPLE: adc_pattern[0].channel is :2
I (377) EXAMPLE: adc_pattern[0].unit is :0
I (387) EXAMPLE: adc_pattern[1].atten is :0
I (387) EXAMPLE: adc_pattern[1].channel is :3
I (397) EXAMPLE: adc_pattern[1].unit is :0
I (397) gpio: GPIO[3]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (407) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (417) TASK: ret is 0, ret_num is 256
I (427) EXAMPLE: Unit: ADC-1, Channel: 2, Value: 819
I (427) EXAMPLE: Unit: ADC-1, Channel: 3, Value: 7b9
I (437) EXAMPLE: Unit: ADC-1, Channel: 2, Value: 7ab
I (437) EXAMPLE: Unit: ADC-1, Channel: 3, Value: 74b
I (447) EXAMPLE: Unit: ADC-1, Channel: 2, Value: 74d
I (447) EXAMPLE: Unit: ADC-1, Channel: 3, Value: 6e5
I (457) EXAMPLE: Unit: ADC-1, Channel: 2, Value: 6ee
I (467) EXAMPLE: Unit: ADC-1, Channel: 3, Value: 680
I (467) EXAMPLE: Unit: ADC-1, Channel: 2, Value: 69a
I (477) EXAMPLE: Unit: ADC-1, Channel: 3, Value: 62f
...
```

## Troubleshooting

* program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
