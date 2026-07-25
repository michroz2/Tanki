/**
 * @file audio_system.h
 * @version 0.10
 * @brief Подсистема управления звуком по шине I2S.
 * * Добавлены режимы MUTE (для полного отключения) и START (для запуска двигателя).
 */

 #pragma once

 #include <Arduino.h>
 
 // Режимы работы аудио-ядра
 enum AudioMode {
     AUDIO_MODE_MUTE,    // Полная тишина (Тумблер SWA в положении 1)
     AUDIO_MODE_SIREN,   // Тревожная сирена (стики не по центру или нет связи)
     AUDIO_MODE_START,   // Звук стартера (воспроизведение start.wav)
     AUDIO_MODE_ENGINE   // Нормальная работа (воспроизведение idle.wav)
 };
 
 void initAudio();
 void updateEngineSound(int throttle);
 
 /**
  * @brief Переключает режим работы аудио-ядра.
  * @param mode Желаемый режим.
  */
 void setAudioMode(AudioMode mode);