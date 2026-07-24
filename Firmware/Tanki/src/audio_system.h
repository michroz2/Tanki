/**
 * @file audio_system.h
 * @version 0.9
 * @brief Подсистема управления звуком по шине I2S.
 * * Добавлена поддержка режимов (Сирена / Двигатель) для безопасного старта.
 */

 #pragma once

 #include <Arduino.h>
 
 // Режимы работы аудио-ядра
 enum AudioMode {
     AUDIO_MODE_INIT,    // Ожидание
     AUDIO_MODE_SIREN,   // Тревожная сирена (стики не по центру)
     AUDIO_MODE_ENGINE   // Нормальная работа (звук мотора)
 };
 
 void initAudio();
 void updateEngineSound(int throttle);
 
 /**
  * @brief Переключает режим работы аудио-ядра.
  * @param mode Желаемый режим (Сирена или Двигатель).
  */
 void setAudioMode(AudioMode mode);
 