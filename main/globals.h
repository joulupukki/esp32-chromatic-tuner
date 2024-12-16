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

/**
 * The purpose of the globals file is to keep track of all variables that are
 * accessible by the entire application. In addtion, synchornization
 * convenience functions are places around these variables so that the two
 * cores of ESP32 can safely read/write to these variables.
 */

#if !defined(TUNER_GLOBALS)
#define TUNER_GLOBALS

typedef enum {
    NOTE_C = 0,
    NOTE_C_SHARP,
    NOTE_D,
    NOTE_D_SHARP,
    NOTE_E,
    NOTE_F,
    NOTE_F_SHARP,
    NOTE_G,
    NOTE_G_SHARP,
    NOTE_A,
    NOTE_A_SHARP,
    NOTE_B,
    NOTE_NONE
} TunerNoteName;

/// @brief Gets the currently-detected frequency (thread safe).
/// @return The currently-detected frequency or -1 if no frequency is detected.
float get_current_frequency();

/// @brief Sets a newly-detected frequency (thread safe).
/// @param new_frequency The newly-detected frequency or -1 if no frequency is detected.
void set_current_frequency(float new_frequency);

#endif