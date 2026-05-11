# MCU-Projects 🛠️

A collection of professional and experimental firmware for microcontrollers (ESP8266, ESP32, Arduino). This repository showcases various implementations of IoT solutions, Telegram-driven interfaces, and interactive animations with a focus on clean C++ code and unique aesthetics.

---

## 🚀 Projects Overview

### 🐱 Omega Kerfur (Omega Керфур)
An interactive animated terminal inspired by "Voices of the Void". 
* Features: Standard "OwO" face with realistic blinking logic (random intervals).
* Interactivity: Responds to the /pet command via Telegram, changing the expression to ">w<" for 5 seconds.
* Display: Optimized for SSD1306 OLED, using only the blue zone for a clean look.

### 💬 TgMessageWithPresets
Advanced Telegram-to-OLED interface.
* Features: Integrated Reply Keyboard with presets (UwU, :3, Time, Currency, etc.).
* UI: All messages are perfectly centered in the blue zone of the display.
* Versatility: Handles both quick presets and custom text messages.

### ✉️ TGmessageOnDisplay
A simplified logging terminal.
* Features: Displays incoming Telegram messages in a "Notification" style.
* Format: Shows From: @username followed by the message text.

### 🔔 Reminderbot
A utility script for managing reminders and notifications directly on your hardware display.

---

## 🔌 Hardware Setup

* Controller: ESP8266 (NodeMCU/Wemos D1 Mini)
* Display: OLED 0.96" I2C (SSD1306)
* Wiring: * SDA -> D5
  * SCL -> D6
  * VCC -> 3.3V
  * GND -> GND

---

## 🛠️ Libraries Used
* UniversalTelegramBot - for Telegram API integration.
* ESP8266-OLED-SSD1306 - for display management.
* ArduinoJson - for parsing bot data.

---

## 🇷🇺 Описание проектов (Russian)

### 🐱 Omega Kerfur (Омега Керфур)
Интерактивный анимированный терминал, вдохновленный игрой "Voices of the Void".
* Особенности: Стандартное лицо "OwO" с логикой реалистичного моргания (случайные интервалы).
* Интерактив: Реагирует на команду /pet в Telegram, меняя эмоцию на ">w<" на 5 секунд.

### 💬 TgMessageWithPresets
Продвинутый интерфейс для вывода сообщений из Telegram.
* Особенности: Встроенная клавиатура с пресетами (UwU, время, курс валют и т.д.).
* Интерфейс: Весь текст выводится строго по центру в синей зоне дисплея.

### ✉️ TGmessageOnDisplay
Упрощенный терминал уведомлений. Выводит отправителя (@username) и текст сообщения.

---

## 📱 Connect with me / Мои соцсети

* Main Telegram: @MadeBySuperMaxVF (https://t.me/MadeBySuperMaxVF) — News and updates.
* Dev Telegram: @Mad3BySuperMaxVF (https://t.me/Mad3BySuperMaxVF) — Programming and tech stuff.
* YouTube: SuperMaxVF (https://youtube.com/@supermaxvf?si=pU9QY4a9GZ4ty2FW)
* TikTok: @supermaxvfvf (https://www.tiktok.com/@supermaxvfvf?_r=1&_t=ZT-96GXCdCg9ef)

---
*Developed with ❤️ by SuperMaxVF*
