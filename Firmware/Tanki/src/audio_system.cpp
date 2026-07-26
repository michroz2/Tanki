/**
 * @file audio_system.cpp
 * @version 0.11
 * @brief Многопоточная подсистема I2S-аудио.
 * * Имплементировано безопасное переключение контекста (Thread-Safe I2S).
 * * Автоматическая склейка файлов (start.wav -> idle.wav).
 * * Новая формула громкости для сирены (VRA + MAX) / 2.
 */

 #include "audio_system.h"
 #include <driver/i2s.h>
 #include <LittleFS.h>
 #include <math.h>
 
 const i2s_port_t I2S_PORT = I2S_NUM_0;
 const int BASE_SAMPLE_RATE = 16000;
 const int MAX_SAMPLE_RATE = 26000;
 const int PIN_I2S_BCLK = 26;
 const int PIN_I2S_LRC = 25;
 const int PIN_I2S_DIN = 27;
 
 TaskHandle_t audioTaskHandle = NULL;
 
 // Глобальные "целевые" переменные. Ядро 1 только пишет сюда, Ядро 0 читает и применяет.
 volatile AudioMode targetAudioMode = AUDIO_MODE_MUTE;
 volatile uint32_t targetSampleRate = BASE_SAMPLE_RATE;
 volatile float currentVolume = 1.0f;
 
 void setAudioVolume(float volume) {
     currentVolume = constrain(volume, 0.0f, 1.0f);
 }
 
 // Теперь функция не дергает I2S аппаратно, она просто ставит задачу для Ядра 0
 void setAudioMode(AudioMode mode) {
     targetAudioMode = mode;
     if (mode == AUDIO_MODE_SIREN || mode == AUDIO_MODE_MUTE || mode == AUDIO_MODE_START || mode == AUDIO_MODE_STOP) {
         targetSampleRate = BASE_SAMPLE_RATE;
     }
 }
 
 void updateEngineSound(int throttle) {
     if (targetAudioMode != AUDIO_MODE_ENGINE) return;
 
     int diff = 0;
     uint32_t newRate = BASE_SAMPLE_RATE;
 
     if (throttle > 1530) diff = throttle - 1530;
     else if (throttle < 1470) diff = 1470 - throttle;
 
     if (diff > 0) {
         newRate = BASE_SAMPLE_RATE + map(diff, 0, 470, 0, (MAX_SAMPLE_RATE - BASE_SAMPLE_RATE));
         newRate = constrain(newRate, BASE_SAMPLE_RATE, MAX_SAMPLE_RATE);
     }
     
     // Просто обновляем целевую частоту
     targetSampleRate = newRate;
 }
 
 void audioTask(void *pvParameters) {
     File audioFile;
     // Локальные "активные" переменные Ядра 0
     AudioMode activeAudioMode = AUDIO_MODE_MUTE;
     uint32_t activeSampleRate = BASE_SAMPLE_RATE;
 
     const int numSamples = 128; 
     int16_t outSamples[numSamples * 2]; 
     uint8_t fileBuffer[numSamples * 2]; 
 
     uint32_t sirenTimeCounter = 0;
     float phase = 0.0;
 
     while (true) {
         // 1. ПРОВЕРКА ЗАДАЧ ОТ ЯДРА 1 (Смена режима или частоты)
         if (activeAudioMode != targetAudioMode || activeSampleRate != targetSampleRate) {
             
             if (activeSampleRate != targetSampleRate) {
                 activeSampleRate = targetSampleRate;
                 i2s_set_sample_rates(I2S_PORT, activeSampleRate);
             }
 
             if (activeAudioMode != targetAudioMode) {
                 Serial.printf("[AUDIO CORE 0] Переход режима: %d -> %d\n", activeAudioMode, targetAudioMode);
                 activeAudioMode = targetAudioMode;
                 sirenTimeCounter = 0;
                 phase = 0.0;
                 
                 if (audioFile) audioFile.close();
                 
                 if (activeAudioMode == AUDIO_MODE_START) {
                     audioFile = LittleFS.open("/start.wav", "r");
                     if (audioFile) audioFile.seek(44);
                 } else if (activeAudioMode == AUDIO_MODE_ENGINE) {
                     audioFile = LittleFS.open("/idle.wav", "r");
                     if (audioFile) audioFile.seek(44);
                 } else if (activeAudioMode == AUDIO_MODE_STOP) {
                     audioFile = LittleFS.open("/stop.wav", "r");
                     if (audioFile) audioFile.seek(44);
                 }
             }
         }
 
         // 2. ВОСПРОИЗВЕДЕНИЕ
         if (activeAudioMode == AUDIO_MODE_MUTE) {
             memset(outSamples, 0, sizeof(outSamples));
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, sizeof(outSamples), &bytesWritten, portMAX_DELAY);
         } 
         else if (activeAudioMode == AUDIO_MODE_SIREN) {
             // НОВАЯ ФОРМУЛА: Громкость = (Текущая + Максимум) / 2
             float sirenVol = (currentVolume + 1.0f) / 2.0f;
             
             for (int i = 0; i < numSamples; i++) {
                 int16_t sample = 0;
                 if (sirenTimeCounter < 8000) { 
                     float phaseInc = (2.0f * PI * 600.0) / BASE_SAMPLE_RATE;
                     sample = (int16_t)(sin(phase) * 8000.0 * sirenVol);
                     phase += phaseInc;
                 } else if (sirenTimeCounter < 16000) { 
                     float phaseInc = (2.0f * PI * 400.0) / BASE_SAMPLE_RATE;
                     sample = (int16_t)(sin(phase) * 8000.0 * sirenVol);
                     phase += phaseInc;
                 } else if (sirenTimeCounter < 48000) { 
                     sample = 0; phase = 0;
                 } else {
                     sirenTimeCounter = 0; 
                 }
                 if (phase >= 2.0f * PI) phase -= 2.0f * PI;
                 outSamples[i * 2] = sample;       
                 outSamples[i * 2 + 1] = sample;   
                 sirenTimeCounter++;
             }
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, numSamples * 4, &bytesWritten, portMAX_DELAY);
         } 
         else if (activeAudioMode == AUDIO_MODE_START || activeAudioMode == AUDIO_MODE_ENGINE || activeAudioMode == AUDIO_MODE_STOP) {
             size_t bytesRead = 0;
             if (audioFile) {
                 bytesRead = audioFile.read(fileBuffer, sizeof(fileBuffer));
             }
 
             if (bytesRead > 0) {
                 int samplesRead = bytesRead / 2; 
                 int16_t *rawSamples = (int16_t *)fileBuffer;
 
                 for (int i = 0; i < samplesRead; i++) {
                     int16_t scaledSample = (int16_t)(rawSamples[i] * currentVolume);
                     outSamples[i * 2] = scaledSample;       
                     outSamples[i * 2 + 1] = scaledSample;   
                 }
                 size_t bytesWritten;
                 i2s_write(I2S_PORT, outSamples, samplesRead * 4, &bytesWritten, portMAX_DELAY);
             } else {
                 // КОНЕЦ ФАЙЛА ДОСТИГНУТ: Автоматическая логика переключений
                 if (activeAudioMode == AUDIO_MODE_START) {
                     Serial.println("[AUDIO CORE 0] start.wav закончился. Склейка -> idle.wav");
                     targetAudioMode = AUDIO_MODE_ENGINE;
                 } else if (activeAudioMode == AUDIO_MODE_ENGINE) {
                     if (audioFile) audioFile.seek(44); // Зацикливаем звук мотора
                 } else if (activeAudioMode == AUDIO_MODE_STOP) {
                     Serial.println("[AUDIO CORE 0] stop.wav закончился. Переход в MUTE");
                     targetAudioMode = AUDIO_MODE_MUTE;
                 } else {
                     memset(outSamples, 0, sizeof(outSamples));
                     size_t bytesWritten;
                     i2s_write(I2S_PORT, outSamples, sizeof(outSamples), &bytesWritten, portMAX_DELAY);
                 }
             }
         }
     }
 }
 
 void initAudio() {
     if (!LittleFS.begin(true)) {
         Serial.println("AUDIO ERROR: LittleFS Mount Failed");
         return;
     }
     i2s_config_t i2s_config = {
         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
         .sample_rate = BASE_SAMPLE_RATE,
         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
         .communication_format = I2S_COMM_FORMAT_STAND_I2S,
         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
         .dma_buf_count = 4,
         .dma_buf_len = 256,
         .use_apll = true,
         .tx_desc_auto_clear = true
     };
     i2s_pin_config_t pin_config = {
         .bck_io_num = PIN_I2S_BCLK,
         .ws_io_num = PIN_I2S_LRC,
         .data_out_num = PIN_I2S_DIN,
         .data_in_num = I2S_PIN_NO_CHANGE
     };
     i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
     i2s_set_pin(I2S_PORT, &pin_config);
     i2s_zero_dma_buffer(I2S_PORT);
     
     // Запуск на Ядре 0 с приоритетом 1
     xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 1, &audioTaskHandle, 0);
 }