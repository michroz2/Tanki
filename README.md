# Tanki – ESP32 RC Tank Firmware (1:16 Scale)

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino/PlatformIO](https://img.shields.io/badge/Framework-PlatformIO-orange.svg)](https://platformio.org/)
[![RTOS: FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green.svg)](https://www.freertos.org/)
[![Status: Active Development](https://img.shields.io/badge/Status-Active_Development-brightgreen.svg)]()

*For the Russian version, scroll down or click [here](#описание-на-русском-языке-ru).*

## 🇬🇧 Overview (EN)

**Tanki** is a custom firmware designed for 1:16 scale radio-controlled tanks. The system runs on an ESP32 microcontroller utilizing FreeRTOS, replacing stock control boards with a multi-threaded embedded architecture.

### Core Features

* **Digital Protocol:** Direct parser for the FlySky i-BUS protocol, reading 10-channel data frames from the FS-iA6B receiver via UART.
* **Motor Mixing:** Independent dual-track PWM motor control with a proportional mixer for smooth driving, acceleration, and pivot turns.
* **Turret Control:** Dedicated logic for turret rotation (gun elevation is currently postponed).
* **Signal Safety:** Integrated emergency counter mechanism that locks track movement upon signal loss.
* **Audio Subsystem:** Multi-threaded FreeRTOS implementation for non-blocking I2S audio playback via the MAX98357A DAC amplifier.

### Tank Behavior & Radio Control Mapping

Control is performed using a FlySky transmitter (Mode 2 configuration):

* **Right Stick (Vertical):** Throttle — controls forward and reverse movement.
* **Right Stick (Horizontal):** Steering — controls differential track speeds for turns and pivot rotations.
* **Left Stick (Horizontal):** Turret rotation — proportional control of the turret motor.
* **Gun Elevation:** Temporarily postponed (control channel currently inactive).

---

## 🇷🇺 Описание на русском языке (RU)

**Tanki** — это кастомная прошивка для радиоуправляемых моделей танков масштаба 1:16. Система функционирует на микроконтроллере ESP32 под управлением операционной среды FreeRTOS, заменяя штатные платы управления многопоточной встраиваемой архитектурой.

### Основные возможности

* **Цифровой протокол:** Собственный парсер протокола FlySky i-BUS, считывающий 10-канальные пакеты данных с приёмника FS-iA6B по интерфейсу UART.
* **Управление моторами:** Независимое ШИМ-управление двумя гусеницами через пропорциональный микшер для плавного хода, разгона и разворотов на месте.
* **Управление башней:** Логика вращения башни (подъём орудия в текущей версии временно отложен).
* **Безопасность связи:** Встроенный механизм счетчика аварийных состояний (emergency counter), блокирующий ходовую часть при прерывании управляющего сигнала.
* **Аудиоподсистема:** Многопоточная задача FreeRTOS для неблокирующего воспроизведения звука через I2S ЦАП MAX98357A.

### Поведение танка и назначение каналов пульта

Управление осуществляется с пульта ДУ FlySky (раскладка стиков Mode 2):

* **Правый стик (вертикаль):** Газ — движение вперед и назад.
* **Правый стик (горизонталь):** Поворот — дифференциальное управление гусеницами для выполнения плавных маневров и разворотов на месте.
* **Левый стик (горизонталь):** Поворот башни — пропорциональное управление приводом башни.
* **Подъём орудия:** Временно отложен (соответствующий канал в текущей версии прошивки не задействован).