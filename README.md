# Introduction to Robotics (2026 - 2027)

This repository contains my homework for the **Introduction to Robotics** course, taken in my 3rd year at the Faculty of Mathematics and Computer Science, University of Bucharest.

Each homework is in its own folder and has its own README. Every README describes the task, how I implemented it, the calculations and schematics behind it, and a demo (screenshots, simulation files, or a video, depending on the homework).

## Homeworks

| # | Topic | Folder | Status | Bonuses |
|---|-------|--------|--------|---------|
| 1 | Lab group registration (submitted on Teams) | - | ✅ Done | – |
| 2 | Basic Electrical Circuits (Falstad simulations: series, parallel and RC circuits) | [Homework 2](./Homework%202/) | ✅ Done | Parallel circuit with a switch |
| 3 | Fan Control and Status Panel (ESP32: PWM, ADC, RGB LED, buzzer, non-blocking timing) | [Homework 3](./Homework%203/) | ✅ Done | – |
| 4 | Smart Lamp Controller (ESP32: pushbutton modes, debouncing, short/long press, interrupts, PWM) | [Homework 4](./Homework%204/) | ✅ Done | B: BREATHING mode · C: raw interrupts vs. accepted presses |
| 5 | Stop the Counter! (ESP32: 74HC595 shift register, 4-digit 7-segment multiplexing, joystick, bit masks, non-blocking state machine) | [Homework 5](./Homework%205/) | ✅ Done | – (coming) |
| 6 | - | [Homework 6](./Homework%206/) | Not assigned yet | |
| 7 | - | [Homework 7](./Homework%207/) | Not assigned yet | |
| 8 | - | [Homework 8](./Homework%208/) | Not assigned yet | |
| 9 | - | [Homework 9](./Homework%209/) | Not assigned yet | |
| 10 | - | [Homework 10](./Homework%2010/) | Not assigned yet | |
| 11 | - | [Homework 11](./Homework%2011/) | Not assigned yet | |
| 12 | - | [Homework 12](./Homework%2012/) | Not assigned yet | |

## Repository structure

Every homework folder follows the same layout. Supporting files always go in subfolders:

```
Homework N/
├── README.md        description, calculations, results and theory vs. practice
├── <Project>/       PlatformIO project (hardware homeworks): platformio.ini, src/main.cpp
├── circuits/        Falstad simulation exports, *.txt (simulation homeworks)
├── images/          photos, diagrams and screenshots used in the README
└── calculations/    handwritten calculations, *.pdf (when there are any)
```

Hardware homeworks contain a PlatformIO project (`platformio.ini` + `src/main.cpp`) instead of an Arduino `*.ino` sketch.

## Tools used

- [Falstad Circuit Simulator](https://www.falstad.com/circuit/)
- [PlatformIO](https://platformio.org/) (VS Code extension)
- ESP32 (ESP32-WROOM DevKit) with the Arduino framework

## Acknowledgements

External code, resources, and significant AI or colleague assistance are credited in the README of the homework where they were used.
