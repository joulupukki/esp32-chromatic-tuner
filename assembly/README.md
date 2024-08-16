# Overview

The tuner code needs a circuit to do a few things. Primarily:
- Offset the signal voltage to put it in the center of the esp32 3.1v range
- Filter noise in various ways

## Voltage divider offset voltage
The circuit runs on 5V which can be sourced from USB or an external source.
From there it must be stepped down to within the range of the esp32. R1 and R2 make up a voltage divider that puts the center of the signal at 1.56v.

## Signal filtering
[R5 and C3 form a LPF] (https://en.wikipedia.org/wiki/Low-pass_filter) at 590 Hz  
R3 and C2 form a HPF at 53 Hz

## Voltage regulation
While USB power can be used, a 7805 allows the circuit to operate off of an external 9v power supply. C10-13 provide noise filtering.
D1 is polarity protection. D2 protects the regulator when powering via USB.
