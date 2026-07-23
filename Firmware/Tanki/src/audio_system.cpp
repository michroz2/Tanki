/**
 * @file audio_system.cpp
 * @version 0.7
 * @brief Реализация подсистемы управления звуком I2S с чтением WAV из LittleFS.
 */

#include "audio_system.h"
#include <driver/i2s.h>
#include <LittleFS.h>

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int SAMPLE_RATE = 16000;
const int PIN_I2S_BCLK = 26;
const int PIN_I2S_LRC = 25;
const int PIN_I2S_DIN = 27;

TaskHandle_t audioTaskHandle = NULL;

/**
 * @brief Фоновая задача для Ядра 0: Чтение WAV и отправка в I2S
 */
void audioTask(void *pvParameters) {
    // Открываем файл холостого хода
    File audioFile = LittleFS.open("/idle.wav", "r");
    if (!audioFile) {
        Serial.println("AUDIO ERROR: /idle.wav not found!");
        vTaskDelete(NULL); // Уничтожаем задачу, если файла нет
        return;
    }

    const int numSamples = 128; // Размер чанка
    int16_t outSamples[numSamples * 2]; // Буфер для I2S (Стерео: L + R)
    uint8_t fileBuffer[numSamples * 2]; // Буфер для чтения файла (Моно 16-bit = 2 байта на сэмпл)

    // Пропускаем стандартный 44-байтный WAV-заголовок
    audioFile.seek(44);

    while (true) {
        // Если до конца файла осталось меньше одного полного буфера — зацикливаем
        if (audioFile.available() < sizeof(fileBuffer)) {
            audioFile.seek(44); 
        }

        // Читаем блок данных из флеш-памяти
        size_t bytesRead = audioFile.read(fileBuffer, sizeof(fileBuffer));
        int samplesRead = bytesRead / 2; // 2 байта на 1 сэмпл (16-bit)
        
        // Преобразуем массив байт в массив 16-битных целых чисел
        int16_t *rawSamples = (int16_t *)fileBuffer;

        // Дублируем Моно-сигнал в Левый и Правый каналы I2S
        for (int i = 0; i < samplesRead; i++) {
            outSamples[i * 2] = rawSamples[i];       // Левый
            outSamples[i * 2 + 1] = rawSamples[i];   // Правый
        }

        size_t bytesWritten;
        // Блокирующая передача в DMA
        i2s_write(I2S_PORT, outSamples, samplesRead * 4, &bytesWritten, portMAX_DELAY);
    }
}

void initAudio() {
    // Инициализация файловой системы
    if (!LittleFS.begin(true)) {
        Serial.println("AUDIO ERROR: LittleFS Mount Failed");
        return; // Прерываем инициализацию аудио, если FS не работает
    }

    // Настройка I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
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

    // Запуск задачи на Ядре 0
    xTaskCreatePinnedToCore(
        audioTask,
        "AudioTask",
        4096,
        NULL,
        1,
        &audioTaskHandle,
        0
    );
}