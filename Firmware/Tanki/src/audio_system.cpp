/**
 * @file audio_system.cpp
 * @version 0.6
 * @brief Реализация подсистемы управления звуком I2S.
 * * В текущей версии реализована генерация чистого тестового 
 * синусоидального тона (440 Гц) для проверки аппаратной части
 * (модуля MAX98357A) и стабильности многопоточной архитектуры.
 */

 #include "audio_system.h"
 #include <driver/i2s.h>
 #include <math.h>
 
 // Аппаратные константы I2S
 const i2s_port_t I2S_PORT = I2S_NUM_0;    // Используем нулевой порт I2S
 const int SAMPLE_RATE = 16000;            // Частота дискретизации 16 кГц
 const int PIN_I2S_BCLK = 26;              // Пин тактирования битов (BCLK)
 const int PIN_I2S_LRC = 25;               // Пин выбора канала (LRC/WS)
 const int PIN_I2S_DIN = 27;               // Пин вывода данных (DIN)
 
 TaskHandle_t audioTaskHandle = NULL;      // Хэндл для управления задачей FreeRTOS
 
 /**
  * @brief Фоновая задача FreeRTOS для генерации аудиосигнала.
  * Жестко привязана к Ядру 0. Генерирует непрерывный тон.
  * * @param pvParameters Указатель на параметры задачи (не используется)
  */
 void audioTask(void *pvParameters) {
     // Параметры тестового тона (Нота Ля, 440 Гц)
     const float frequency = 440.0;
     const float amplitude = 8000.0; // Громкость (от 0 до 32767 для 16 бит)
     float phase = 0.0;
     const float phaseIncrement = (2.0f * PI * frequency) / SAMPLE_RATE;
 
     // Инициализация DMA-буфера. Размер 128 сэмплов.
     // Умножаем на 2, так как I2S ожидает стерео-формат данных (Левый + Правый канал)
     const int numSamples = 128;
     int16_t samples[numSamples * 2]; 
 
     while (true) {
         // Заполнение буфера математической синусоидой
         for (int i = 0; i < numSamples; i++) {
             int16_t sample = (int16_t)(sin(phase) * amplitude);
             samples[i * 2] = sample;       // Левый канал
             samples[i * 2 + 1] = sample;   // Правый канал
             
             phase += phaseIncrement;
             if (phase >= 2.0f * PI) {
                 phase -= 2.0f * PI;
             }
         }
 
         size_t bytesWritten;
         
         // Передача данных в DMA-контроллер I2S.
         // Параметр portMAX_DELAY критически важен: он переводит задачу
         // в режим ожидания (Blocked) до момента аппаратного освобождения буфера.
         // Это предотвращает срабатывание сторожевого таймера (Watchdog).
         i2s_write(I2S_PORT, samples, sizeof(samples), &bytesWritten, portMAX_DELAY);
     }
 }
 
 void initAudio() {
     // Структура конфигурации параметров шины I2S
     i2s_config_t i2s_config = {
         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // ESP32 - мастер, только передача
         .sample_rate = SAMPLE_RATE,                          // 16 000 Гц
         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,        // Разрядность 16 бит
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,        // Стерео формат
         .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Стандартный протокол I2S
         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Приоритет прерывания
         .dma_buf_count = 4,                                  // Количество DMA буферов
         .dma_buf_len = 256,                                  // Длина каждого буфера
         .use_apll = false,                                   // Использовать стандартный PLL
         .tx_desc_auto_clear = true                           // Очистка от шума при нехватке данных
     };
 
     // Структура конфигурации пинов I2S
     i2s_pin_config_t pin_config = {
         .bck_io_num = PIN_I2S_BCLK,
         .ws_io_num = PIN_I2S_LRC,
         .data_out_num = PIN_I2S_DIN,
         .data_in_num = I2S_PIN_NO_CHANGE // Входные данные не используются
     };
 
     // Применение конфигурации на аппаратном уровне
     i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
     i2s_set_pin(I2S_PORT, &pin_config);
     i2s_zero_dma_buffer(I2S_PORT); // Принудительное обнуление для тишины при старте
 
     // Запуск задачи в операционной системе FreeRTOS
     xTaskCreatePinnedToCore(
         audioTask,        // Функция, реализующая задачу
         "AudioTask",      // Имя задачи
         4096,             // Выделенный стек (4 КБ)
         NULL,             // Параметры не передаются
         1,                // Низкий приоритет, чтобы не мешать критическим задачам
         &audioTaskHandle, // Указатель на хэндл
         0                 // Привязка к Ядру 0 (PRO_CPU)
     );
 }