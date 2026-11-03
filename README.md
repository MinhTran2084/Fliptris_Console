# Fliptris Console

A modified Tetris game on an 8x16 LED grid with extra physical rotation mechanics and a custom PC GUI.

This repository serves as the project archive for the ENGG2800 Team Project at the University of Queensland, containing all hardware design files, firmware, and companion software.

## 🎮 Features
* **Custom PCB:** Hand-assembled printed circuit board prioritising surface-mounted devices (SMDs).
* **8x16 LED Playfield:** Driven by two MAX7219 display ICs to ensure uniform brightness.
* **Physical Rotation Mechanics:** Integrates an LIS3DH accelerometer to detect physical console rotation, influencing in-game block movement.
* **SNES Controller Interface:** Hardware support for standard SNES gamepad inputs.
* **Custom PC GUI:** A companion desktop application that handles scoring, next-block previews, scrolling text, and a dedicated diagnostic test mode.
* **Audio Feedback:** Integrated piezo buzzer for tone generation.

## 🗂️ Repository Structure
* `/Hardware_Altium/` - Source Altium Designer files (Schematics and PCB Layout).
* `/Hardware_Gerbers/` - Exported manufacturing files used for PCB fabrication.
* `/Software/` - Firmware for the microcontroller and the companion PC GUI application.
* `/Media/` - Photos of the physical PCB assembly and MP4 video demonstrations of gameplay.

## 🛠️ Hardware Stack
* **Main Microcontroller:** AVR128DB28
* **PC Communication Bridge:** Seeeduino Xiao (running polyglot-turtle-xiao firmware)
* **Display Drivers:** 2x MAX7219 ICs
* **Sensor:** LIS3DH Accelerometer
* **Displays:** 2x 8x8 LED Matrices
* **Buzzer:** Buzzer Piezo 3V 12.2MM TH

## 👥 Team 33 (ENGG2800 - Semester 2, 2026)
* **Ngoc Minh Tran:** Hardware Engineering (Prototyping, Schematics, PCB Design)
* **Mihiru Dissanayake:** Firmware (Buzzer, Accelerometer logic, Inputs)
* **Sewon Kim:** Game Logic (Bag randomiser, Block generation, LED matrix logic)
* **Thanabammini Balasaravanan:** PC GUI (Scoring, Block preview, Test mode)
