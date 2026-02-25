# ESP32 Embedded Learning Project (ESP-IDF + PlatformIO)

Hands-on embedded systems learning project built on **ESP32 DevKitC (WROOM)** using **ESP-IDF** in **PlatformIO (VS Code)**.

This repository was used as a practical sandbox to learn core embedded software engineering concepts on real hardware. It started small and gradually grew into a larger multi-topic experiment, so it is now kept as a **reference/archive**, while future learning continues in a new, cleaner repo.

---

## Goals

Learn embedded development by building real features with:
- C/C++
- ESP32 + ESP-IDF
- FreeRTOS
- Sensors, peripherals, and communication protocols
- Debugging and testing mindset

---

## Hardware Used

- **ESP32 DevKitC (WROOM)**
- LEDs, resistors, button, breadboard, jumper wires
- **SSD1306 OLED (I2C)**
- **SHT31 / SHT30 temperature & humidity sensor (I2C)**
- **SPL06-001 pressure sensor (SPI)**
- **SD card module (SPI)**
- ADC/PWM test components (e.g., potentiometer / analog input)
- **ESP-Prog (JTAG debugger)**
- **USB Logic Analyzer (24MHz, 8-channel)**

---

## Software Stack

- **Language:** C++
- **Framework:** ESP-IDF
- **Build/IDE:** PlatformIO + VS Code
- **RTOS:** FreeRTOS (via ESP-IDF)

---

## Topics Practiced

- **FreeRTOS tasks, priorities, delays**
- **Queues** (value queues, pointer queues, queue sets)
- **Mutexes and critical sections**
- **Task notifications**
- **Watchdog (TWDT) + health/heartbeat monitoring**
- **GPIO + interrupts (button ISR)**
- **Debouncing + short/long press logic**
- **UART command interface / CLI-style commands**
- **FreeRTOS software timers**
- **I2C communication** (scan + sensor + OLED)
- **SPI communication** (sensor + SD card basics)
- **ADC + PWM experiments**
- **Refactoring into OOP (`App`, `AppContext`, trampolines)**
- **Basic unit testing mindset** (parser extraction / native tests)
- **Debugging workflow** (logs, watchdog traces, JTAG setup)
- **Logic analyzer usage** for signal/protocol debugging (UART/I2C/SPI timing observation)

---

## Project Nature

This is a **learning project**, so the code includes:
- experiments
- refactors
- debug code
- partially cleaned modules
- commented sections from testing

It is not intended to be a polished production codebase.

---

## Build / Run (PlatformIO)

```bash
pio run
pio run -t upload
pio device monitor
```

---

## Why this repo is archived (for now)

As more features were added, the project became too large for efficient learning.
I’m keeping this repo as a reference and moving to a new repo with a cleaner structure and smaller focused lessons.

