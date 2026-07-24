/**
 * @file audio_system.cpp
 * @version 0.8
 * @brief Реализация подсистемы управления звуком I2S.
 * * Интегрировано изменение тональности звука через i2s_set_sample_rates.
 * Применен алгоритм гистерезиса для защиты Audio PLL от перегрузки.
 */

 #include "audio_system.h"
 #include <driver/i2s.h>
 #include <LittleFS.h>
 
 const i2s_port_t I2S_PORT = I2S_NUM_0;
 const int BASE_SAMPLE_RATE = 16000;       // Базовая частота холостого хода
 const int MAX_SAMPLE_RATE = 26000;        // Максимальная частота (полный газ)
 const int PIN_I2S_BCLK = 26;
 const int PIN_I2S_LRC = 25;
 const int PIN_I2S_DIN = 27;
 
 TaskHandle_t audioTaskHandle = NULL;
 uint32_t currentSampleRate = BASE_SAMPLE_RATE; // Текущая частота воспроизведения
 
 /**
  * @brief Фоновая задача для Ядра 0: Чтение WAV и отправка в I2S
  */
 void audioTask(void *pvParameters) {
     File audioFile = LittleFS.open("/idle.wav", "r");
     if (!audioFile) {
         Serial.println("AUDIO ERROR: /idle.wav not found!");
         vTaskDelete(NULL); 
         return;
     }
 
     const int numSamples = 128; 
     int16_t outSamples[numSamples * 2]; 
     uint8_t fileBuffer[numSamples * 2]; 
 
     // Пропуск WAV-заголовка
     audioFile.seek(44);
 
     while (true) {
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
         .use_apll = true, // Аппаратный таймер Audio PLL включен
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
 
 /**
  * @brief Вычисляет и применяет новую частоту в зависимости от положения стика.
  * Вызывается из основного цикла loop на Ядре 1.
  */
 void updateEngineSound(int throttle) {
     int diff = 0;
     uint32_t targetRate = BASE_SAMPLE_RATE;
 
     // Вычисляем абсолютное отклонение стика от мертвой зоны
     if (throttle > 1530) {
         diff = throttle - 1530; // Движение вперед (0..470)
     } else if (throttle < 1470) {
         diff = 1470 - throttle; // Движение назад (0..470)
     }
 
     // Если есть отклонение - пропорционально повышаем частоту
     if (diff > 0) {
         targetRate = BASE_SAMPLE_RATE + map(diff, 0, 470, 0, (MAX_SAMPLE_RATE - BASE_SAMPLE_RATE));
         targetRate = constrain(targetRate, BASE_SAMPLE_RATE, MAX_SAMPLE_RATE);
     }
 
     // Применяем новую частоту только если она значительно изменилась (> 300 Гц),
     // либо если нужно точно сбросить её до холостых оборотов.
     if (abs((long)targetRate - (long)currentSampleRate) > 300 || 
        (targetRate == BASE_SAMPLE_RATE && currentSampleRate != BASE_SAMPLE_RATE)) {
         
         currentSampleRate = targetRate;
         // Аппаратная смена частоты I2S "на лету"
         i2s_set_sample_rates(I2S_PORT, currentSampleRate);
     }
 }