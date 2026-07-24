/**
 * @file audio_system.h
 * @version 0.8
 * @brief Подсистема управления звуком по шине I2S.
 * * Версия 0.8: Добавлена функция динамического изменения частоты
 * дискретизации (Sample Rate) для имитации набора оборотов двигателя.
 */

 #pragma once

 #include <Arduino.h>
 
 /**
  * @brief Инициализирует шину I2S, LittleFS и запускает фоновую аудио-задачу.
  */
 void initAudio();
 
 /**
  * @brief Динамически изменяет частоту звука двигателя.
  * @param throttle Текущее значение канала газа (обычно от 1000 до 2000).
  */
 void updateEngineSound(int throttle);