/**
 * @file audio_system.cpp
 * @version 1.0
 * @brief Реализация многопоточной подсистемы I2S-аудио для ESP32.
 * * Архитектура построена на базе FreeRTOS. Фоновая задача audioTask привязана 
 * к Ядру 0, что исключает любые конфликты с основным циклом loop() на Ядре 1 
 * и предотвращает межъядерные зависания (Deadlock).
 */

 #include "audio_system.h"
 #include <driver/i2s.h>
 #include <LittleFS.h>
 #include <math.h>
 
 // Аппаратные константы шины I2S и усилителя MAX98357A
 const i2s_port_t I2S_PORT = I2S_NUM_0;
 const int BASE_SAMPLE_RATE = 16000;    // Базовая частота дискретизации (16 кГц)
 const int MAX_SAMPLE_RATE = 26000;     // Максимальная частота дискретизации при полном газе
 const int PIN_I2S_BCLK = 26;           // Пин тактирования бит (Bit Clock)
 const int PIN_I2S_LRC = 25;            // Пин выбора канала (Left/Right Clock / Word Select)
 const int PIN_I2S_DIN = 27;            // Пин данных (Data In)
 
 // Дескриптор задачи FreeRTOS для управления фоновым потоком аудио
 TaskHandle_t audioTaskHandle = NULL;
 
 // Потокобезопасные volatile-переменные для межъядерного обмена (Ядро 1 пишет, Ядро 0 читает)
 volatile AudioMode targetAudioMode = AUDIO_MODE_MUTE;
 volatile uint32_t targetSampleRate = BASE_SAMPLE_RATE;
 volatile float currentVolume = 1.0f;
 
 /**
  * @brief Установка громкости с ограничением диапазона [0.0..1.0].
  */
 void setAudioVolume(float volume) {
     currentVolume = constrain(volume, 0.0f, 1.0f);
 }
 
 /**
  * @brief Безопасный запрос на смену аудиорежима из основного цикла (Ядро 1).
  */
 void setAudioMode(AudioMode mode) {
     targetAudioMode = mode;
     // Сбрасываем частоту на базовую при включении служебных звуков или тишины
     if (mode == AUDIO_MODE_SIREN || mode == AUDIO_MODE_MUTE || mode == AUDIO_MODE_START || mode == AUDIO_MODE_STOP) {
         targetSampleRate = BASE_SAMPLE_RATE;
     }
 }
 
 /**
  * @brief Расчет изменения тональности звука мотора в зависимости от отклонения стика газа.
  */
 void updateEngineSound(int throttle) {
     // Изменяем частоту только если в данный момент активен режим двигателя
     if (targetAudioMode != AUDIO_MODE_ENGINE) return;
 
     int diff = 0;
     uint32_t newRate = BASE_SAMPLE_RATE;
 
     // Вычисляем отклонение стика от нейтрального положения (1500 мкс) в обе стороны
     if (throttle > 1530) diff = throttle - 1530;
     else if (throttle < 1470) diff = 1470 - throttle;
 
     // Если стик отклонен — пропорционально увеличиваем частоту дискретизации
     if (diff > 0) {
         newRate = BASE_SAMPLE_RATE + map(diff, 0, 470, 0, (MAX_SAMPLE_RATE - BASE_SAMPLE_RATE));
         newRate = constrain(newRate, BASE_SAMPLE_RATE, MAX_SAMPLE_RATE);
     }
     
     // Передаем новую целевую частоту на Ядро 0
     targetSampleRate = newRate;
 }
 
 /**
  * @brief Главная фоновая задача аудиосистемы, выполняющаяся строго на Ядре 0.
  */
 void audioTask(void *pvParameters) {
     File audioFile;
     // Локальные переменные состояния фоновой задачи
     AudioMode activeAudioMode = AUDIO_MODE_MUTE;
     uint32_t activeSampleRate = BASE_SAMPLE_RATE;
 
     const int numSamples = 128; 
     int16_t outSamples[numSamples * 2]; // Буфер вывода стерео (L + R)
     uint8_t fileBuffer[numSamples * 2]; // Буфер чтения сырых данных из файла
 
     uint32_t sirenTimeCounter = 0;
     float phase = 0.0;
 
     while (true) {
         // Шаг 1: Синхронизация состояния между Ядром 1 и Ядром 0
         if (activeAudioMode != targetAudioMode || activeSampleRate != targetSampleRate) {
             
             // Если изменилась частота — безопасно перенастраиваем аппаратные часы I2S
             if (activeSampleRate != targetSampleRate) {
                 activeSampleRate = targetSampleRate;
                 i2s_set_sample_rates(I2S_PORT, activeSampleRate);
             }
 
             // Если изменился режим — производим переключение файлов и сброс счетчиков
             if (activeAudioMode != targetAudioMode) {
                 Serial.printf("[AUDIO CORE 0] Переход режима: %d -> %d\n", activeAudioMode, targetAudioMode);
                 activeAudioMode = targetAudioMode;
                 sirenTimeCounter = 0;
                 phase = 0.0;
                 
                 // Закрываем предыдущий файл, если он был открыт
                 if (audioFile) audioFile.close();
                 
                 // Открываем соответствующий WAV-файл из LittleFS и пропускаем заголовок (44 байта)
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
 
         // Шаг 2: Генерация или чтение звукового потока в зависимости от активного режима
         if (activeAudioMode == AUDIO_MODE_MUTE) {
             // Режим MUTE: заполняем аудиопакет нулями (тишина)
             memset(outSamples, 0, sizeof(outSamples));
             size_t bytesWritten;
             i2s_write(I2S_PORT, outSamples, sizeof(outSamples), &bytesWritten, portMAX_DELAY);
         } 
         else if (activeAudioMode == AUDIO_MODE_SIREN) {
             // Режим SIREN: программная генерация двухтональной сирены
             // Формула громкости: среднее арифметическое между текущим VRA и 100% (max)
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
                 outSamples[i * 2] = sample;       // Левый канал
                 outSamples[i * 2 + 1] = sample;   // Правый канал
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
                 // Масштабируем прочитанные сэмплы под текущий уровень громкости пользователя
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
                 // КОНЕЦ ФАЙЛА: Обработка авто-переключений (бесшовная логика)
                 if (activeAudioMode == AUDIO_MODE_START) {
                     Serial.println("[AUDIO CORE 0] start.wav закончился. Склейка -> idle.wav");
                     targetAudioMode = AUDIO_MODE_ENGINE; // Автоматический переход на звук работы мотора
                 } else if (activeAudioMode == AUDIO_MODE_ENGINE) {
                     if (audioFile) audioFile.seek(44); // Зацикливание звука холостого хода
                 } else if (activeAudioMode == AUDIO_MODE_STOP) {
                     Serial.println("[AUDIO CORE 0] stop.wav закончился. Переход в MUTE");
                     targetAudioMode = AUDIO_MODE_MUTE; // После остановки выключаем звук
                 } else {
                     memset(outSamples, 0, sizeof(outSamples));
                     size_t bytesWritten;
                     i2s_write(I2S_PORT, outSamples, sizeof(outSamples), &bytesWritten, portMAX_DELAY);
                 }
             }
         }
     }
 }
 
 /**
  * @brief Первичная инициализация аудио-подсистемы (LittleFS, I2S драйвер, запуск задачи).
  */
 void initAudio() {
     if (!LittleFS.begin(true)) {
         Serial.println("AUDIO ERROR: LittleFS Mount Failed");
         return;
     }
     
     // Конфигурация драйвера I2S
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
     
     // Назначение физических пинов I2S
     i2s_pin_config_t pin_config = {
         .bck_io_num = PIN_I2S_BCLK,
         .ws_io_num = PIN_I2S_LRC,
         .data_out_num = PIN_I2S_DIN,
         .data_in_num = I2S_PIN_NO_CHANGE
     };
     
     i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
     i2s_set_pin(I2S_PORT, &pin_config);
     i2s_zero_dma_buffer(I2S_PORT);
     
     // Создание фоновой задачи FreeRTOS и привязка её строго к Ядру 0
     xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 1, &audioTaskHandle, 0);
 }