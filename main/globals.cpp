/*
 * Copyright (c) 2024 Boyd Timothy. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "globals.h"

#include "freertos/FreeRTOS.h"

float current_frequency = -1.0f;
portMUX_TYPE current_frequency_mutex = portMUX_INITIALIZER_UNLOCKED;

float get_current_frequency() {
    float frequency = -1.0f;
    portENTER_CRITICAL(&current_frequency_mutex);
    frequency = current_frequency;
    portEXIT_CRITICAL(&current_frequency_mutex);
    return frequency;
}

void set_current_frequency(float new_frequency) {
    portENTER_CRITICAL(&current_frequency_mutex);
    current_frequency = new_frequency;
    portEXIT_CRITICAL(&current_frequency_mutex);
}