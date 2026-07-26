/**
 * @file audio_system.h
 * @version 0.11
 * @brief Подсистема управления звуком по шине I2S.
 * * В версии 0.11 добавлена потокобезопасность (защита от Deadlock),
 * новый режим AUDIO_MODE_STOP и авто-склейка звука запуска.
 */

 #pragma once

 #include <Arduino.h>
 
 enum AudioMode {
     AUDIO_MODE_MUTE,    // Полная тишина
     AUDIO_MODE_SIREN,   // Предупредительная сирена (Громкость: среднее между VRA и MAX)
     AUDIO_MODE_START,   // Звук запуска (start.wav). По завершении сам перейдет в ENGINE
     AUDIO_MODE_ENGINE,  // Звук работающего двигателя (idle.wav) с изменением тональности
     AUDIO_MODE_STOP     // Звук остановки (stop.wav). По завершении сам перейдет в MUTE
 };
 
 void initAudio();
 void updateEngineSound(int throttle);
 void setAudioMode(AudioMode mode);
 void setAudioVolume(float volume);