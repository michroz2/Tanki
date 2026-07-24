/**
 * @file audio_system.cpp
 * @version 0.9
 * @brief Реализация подсистемы управления звуком I2S.
 * * Добавлен математический синтезатор сирены.
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
 uint32_t currentSampleRate = BASE_SAMPLE_RATE;
 volatile AudioMode currentAudioMode = AUDIO_MODE_INIT; // Текущий режим
 
 void setAudioMode(AudioMode mode) {
     if (currentAudioMode != mode) {
         currentAudioMode = mode;
         // При включении сирены принудительно сбрасываем частоту I2S на базовую
         if (mode == AUDIO_MODE_SIREN) {
             currentSampleRate = BASE_SAMPLE_RATE;
             i2s_set_sample_rates(I2S_PORT, currentSampleRate);
         }
     }
 }
 
 /**
  * @brief Фоновая задача для Ядра 0
  */
 void audioTask(void *pvParameters) {
     File audioFile = LittleFS.open("/idle.wav", "r");
     if (!audioFile) {
         Serial.println("AUDIO ERROR: /idle.wav not found!");
         vTaskDelete(NULL); 
         return;
     }
     audioFile.seek(44);
 
     const int numSamples = 128; 
     int16_t outSamples[numSamples * 2]; 
     uint8_t fileBuffer[numSamples * 2]; 
 
     // Переменные для математической сирены
     uint32_t sirenTimeCounter = 0;
     float phase = 0.0;
 
     while (true) {
         if (currentAudioMode == AUDIO_MODE_ENGINE) {
             // РЕЖИМ 1: Чтение реального двигателя из файла
             if (audioFile.available() < sizeof(fileBuffer)) {
                 audioFile.seek(44); 
             }
             size_t bytesRead = audioFile.read(fileBuffer, sizeof(fileBuffer));
             int samplesRead = bytesRead / 2; 
             int16_t *rawSamples = (int16_t *)fileBuffer;
 
             for (int i = 0; i < samplesRead; i++) {
                 outSamples[i * 2] = rawSamples[i];       
                 outSamples[i * 2 + 1] = rawSamples[i];   
             }
             
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, samplesRead * 4, &bytesWritten, portMAX_DELAY);
 
         } else if (currentAudioMode == AUDIO_MODE_SIREN) {
             // РЕЖИМ 2: Генерация сирены безопасности "на лету"
             for (int i = 0; i < numSamples; i++) {
                 int16_t sample = 0;
                 
                 // 1 секунда = 16000 сэмплов.
                 if (sirenTimeCounter < 8000) { 
                     // Первые 0.5 сек: Высокая нота (600 Гц)
                     float phaseInc = (2.0f * PI * 600.0) / BASE_SAMPLE_RATE;
                     sample = (int16_t)(sin(phase) * 8000.0);
                     phase += phaseInc;
                 } else if (sirenTimeCounter < 16000) { 
                     // Вторые 0.5 сек: Низкая нота (400 Гц)
                     float phaseInc = (2.0f * PI * 400.0) / BASE_SAMPLE_RATE;
                     sample = (int16_t)(sin(phase) * 8000.0);
                     phase += phaseInc;
                 } else if (sirenTimeCounter < 48000) { 
                     // Следующие 2.0 сек: Пауза (Тишина)
                     sample = 0;
                     phase = 0;
                 } else {
                     // Сброс цикла сирены
                     sirenTimeCounter = 0; 
                 }
 
                 if (phase >= 2.0f * PI) phase -= 2.0f * PI;
 
                 outSamples[i * 2] = sample;       // Левый
                 outSamples[i * 2 + 1] = sample;   // Правый
                 sirenTimeCounter++;
             }
 
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, numSamples * 4, &bytesWritten, portMAX_DELAY);
         } else {
             // РЕЖИМ 3: Ожидание (тишина), пока пульт еще не подключился
             memset(outSamples, 0, sizeof(outSamples));
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, numSamples * 4, &bytesWritten, portMAX_DELAY);
         }
     }
 }
 
 // ... остальной код (initAudio и updateEngineSound) остается без изменений ...
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
     xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 1, &audioTaskHandle, 0);
 }
 
 void updateEngineSound(int throttle) {
     if (currentAudioMode != AUDIO_MODE_ENGINE) return; // Защита: не менять частоту при сирене
 
     int diff = 0;
     uint32_t targetRate = BASE_SAMPLE_RATE;
 
     if (throttle > 1530) diff = throttle - 1530;
     else if (throttle < 1470) diff = 1470 - throttle;
 
     if (diff > 0) {
         targetRate = BASE_SAMPLE_RATE + map(diff, 0, 470, 0, (MAX_SAMPLE_RATE - BASE_SAMPLE_RATE));
         targetRate = constrain(targetRate, BASE_SAMPLE_RATE, MAX_SAMPLE_RATE);
     }
 
     if (abs((long)targetRate - (long)currentSampleRate) > 300 || 
        (targetRate == BASE_SAMPLE_RATE && currentSampleRate != BASE_SAMPLE_RATE)) {
         currentSampleRate = targetRate;
         i2s_set_sample_rates(I2S_PORT, currentSampleRate);
     }
 }