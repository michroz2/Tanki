# Project: Tanki Snapshot V1.0.1

## File: .\src\audio_system.cpp
```cpp
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
```

---

## File: .\src\audio_system.h
```cpp
/**
 * @file audio_system.h
 * @version 1.0
 * @brief Заголовочный файл многопоточной подсистемы I2S-аудио.
 * Описывает интерфейсы управления звуковыми режимами, громкостью и динамикой двигателя.
 */

 #pragma once

 #include <Arduino.h>
 
 /**
  * @brief Перечисление доступных режимов работы аудиосистемы.
  */
 enum AudioMode {
     AUDIO_MODE_MUTE,    ///< Полная тишина (вывод нулей в I2S)
     AUDIO_MODE_SIREN,   ///< Аварийная сирена (громкость: среднее между VRA и максимумом)
     AUDIO_MODE_START,   ///< Проигрывание стартового файла (start.wav) с авто-переходом в idle.wav
     AUDIO_MODE_ENGINE,  ///< Работа двигателя (цикличное воспроизведение idle.wav с изменением тональности)
     AUDIO_MODE_STOP     ///< Звук глушения (stop.wav) с последующим переходом в MUTE
 };
 
 /**
  * @brief Инициализация файловой системы LittleFS и аппаратного драйвера I2S на Ядре 0.
  */
 void initAudio();
 
 /**
  * @brief Динамическое обновление тональности (частоты) звука двигателя по стику газа.
  * @param throttle Текущее значение канала газа (1000..2000 мкс).
  */
 void updateEngineSound(int throttle);
 
 /**
  * @brief Установка целевого режима аудио (потокобезопасная установка флага).
  * @param mode Требуемый режим из перечисления AudioMode.
  */
 void setAudioMode(AudioMode mode);
 
 /**
  * @brief Установка общей громкости аудиосистемы.
  * @param volume Значение в диапазоне от 0.0f (тишина) до 1.0f (максимум).
  */
 void setAudioVolume(float volume);
```

---

## File: .\src\ibus_parser.cpp
```cpp
/**
 * @file ibus_parser.cpp
 * @version 1.0
 * @brief Реализация парсера протокола FlySky i-BUS с защитой от сбоев UART.
 * * Модуль отвечает за инициализацию Serial2, безопасное считывание потока,
 * сборку 32-байтовых пакетов, проверку контрольной суммы и защиту от зависания 
 * при аппаратных сбоях шины.
 */

 #include "ibus_parser.h"

 // Глобальный массив значений каналов. Инициализирован нейтральным центром (1500 мкс).
 int channels[IBUS_MAX_CHANNELS] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
 
 // Статический буфер для накопления сырых байт текущего пакета (строго 32 байта).
 static uint8_t ibusBuffer[32];
 
 /**
  * @brief Инициализация аппаратного интерфейса Serial2 для шины i-BUS.
  * @param rxPin Пин ESP32, подключенный к линии RX приемника.
  */
 void initIBus(int rxPin) {
     // Настройка аппаратного UART: 115200 бод, формат 8N1, TX отключен (-1).
     Serial2.begin(115200, SERIAL_8N1, rxPin, -1);
 }
 
 /**
  * @brief Неблокирующее чтение и разбор пакета i-BUS с защитой от зацикливания.
  * @return true, если получен и верифицирован новый пакет; false в противном случае.
  */
 bool readIBus() {
     // Счетчик для защиты от зависания в бесконечном цикле при аппаратных ошибках UART.
     int emergencyCounter = 0; 
 
     // Вычитываем все доступные байты из буфера за один проход loop()
     while (Serial2.available()) {
         emergencyCounter++;
 
         // Защитная ловушка: если за один цикл вычиталось более 200 байт (сбой драйвера UART),
         // принудительно прерываем цикл, чтобы не "повесить" процессор.
         if (emergencyCounter == 200) {
             Serial.println("\n>>> FATAL ERROR: UART DRIVER STUCK! <<<");
             Serial.println("Infinite loop detected in readIBus(). Breaking out!");
             
             // Аппаратная очистка зависшего буфера
             Serial2.flush(); 
             
             // Выходим из цикла, спасая систему от зависания
             break; 
         }
 
         // Считываем очередной байт из потока
         uint8_t val = Serial2.read();
         static int byteIdx = 0;
 
         // Поиск маркеров заголовка пакета i-BUS: 0x20 (длина) и 0x40 (команда)
         if (byteIdx == 0 && val == 0x20) {
             ibusBuffer[0] = val;
             byteIdx = 1;
         } else if (byteIdx == 1 && val == 0x40) {
             ibusBuffer[1] = val;
             byteIdx = 2;
         } 
         // Накопление полезной нагрузки пакета
         else if (byteIdx >= 2) {
             ibusBuffer[byteIdx] = val;
             byteIdx++;
 
             // Когда пакет полностью собран (достиг 32 байт)
             if (byteIdx >= 32) {
                 byteIdx = 0; // Сбрасываем индекс для следующего пакета
 
                 // Вычисление контрольной суммы (Checksum) по стандарту FlySky
                 uint16_t checksum = 0xFFFF;
                 for (int i = 0; i < 30; i++) {
                     checksum -= ibusBuffer[i];
                 }
 
                 // Чтение контрольной суммы из пакета (Little-Endian)
                 uint16_t rxChecksum = ibusBuffer[30] | (ibusBuffer[31] << 8);
 
                 // Если контрольная сумма сошлась — обновляем массив каналов
                 if (checksum == rxChecksum) {
                     for (int i = 0; i < IBUS_MAX_CHANNELS; i++) {
                         channels[i] = ibusBuffer[2 + i * 2] | (ibusBuffer[3 + i * 2] << 8);
                     }
                     return true;
                 }
             }
         } 
         else {
             // При сбое синхронизации сбрасываем указатель для поиска нового заголовка
             byteIdx = 0;
         }
     }
     return false;
 }
```

---

## File: .\src\ibus_parser.h
```cpp
/**
 * @file ibus_parser.h
 * @version 0.1
 * @brief Модуль парсинга протокола i-BUS для приемников FlySky.
 * * Определяет интерфейс взаимодействия с аппаратной шиной i-BUS.
 * Все комментарии и документация ведутся на русском языке.
 */

 #pragma once
 #include <Arduino.h>
 
 // Максимальное количество каналов, передаваемых в стандартном пакете FlySky i-BUS
 const int IBUS_MAX_CHANNELS = 14;
 
 // Глобальный массив, хранящий текущие значения длительности импульсов каналов (1000...2000 мкс)
 // Доступен для чтения в любой точке программы после успешного парсинга
 extern int channels[IBUS_MAX_CHANNELS];
 
 /**
  * @brief Инициализация аппаратного интерфейса для i-BUS.
  * @param rxPin Физический пин процессора ESP32 для приема данных (линия RX).
  */
 void initIBus(int rxPin);
 
 /**
  * @brief Неблокирующее чтение и парсинг входящих пакетов из буфера UART.
  * @return true, если получен, разобран и успешно прошел проверку контрольной суммы полный пакет данных.
  */
 bool readIBus();
```

---

## File: .\src\main.cpp
```cpp
/**
 * @file main.cpp
 * @version 1.0.1
 * @brief Главный файл прошивки радиоуправляемого танка 1:16 (ESP32).
 * * Содержит реализацию конечного автомата (FSM) по утвержденной спецификации, 
 * алгоритм безопасного инерционного торможения при потере связи, 
 * фикс сохранения пользовательской громкости при дисконнекте (v1.0.1),
 * таймер автоотключения простоя (30 сек) и систему телеметрии.
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 #include "turret_control.h"
 #include "audio_system.h" 
 
 // Аппаратные пины периферии
 const int PIN_SERVO = 14;
 const int PIN_IBUS_RX = 34;
 
 // Зона нечувствительности стиков (Deadband) для предотвращения дребезга в центре
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 /**
  * @brief Возможные состояния системы (Finite State Machine).
  */
 enum SystemState {
     STATE_DISCONNECTED, // Потеряна радиосвязь с пультом
     STATE_DISARMED,     // Связь есть, тумблер предохранителя (SWA) выключен (безопасно)
     STATE_STARTFAIL,    // Ошибка запуска (стик не в центре или SWA был включен при старте)
     STATE_START,        // Процесс запуска двигателя (играет start.wav, управление заблокировано)
     STATE_RUNNING,      // Штатный боевой режим (управление движением и башней)
     STATE_SHUTDOWN      // Процесс остановки/глушения с плавным торможением и блокировкой
 };
 
 // Инициализация стартовых переменных машины состояний
 SystemState currentState = STATE_DISCONNECTED;
 SystemState previousState = STATE_DISCONNECTED; 
 bool isFirstLoop = true;
 
 // Текстовые названия состояний для удобства логирования
 const char* stateNames[] = {"DISCONNECTED", "DISARMED", "STARTFAIL", "START", "RUNNING", "SHUTDOWN"};
 
 // Таймеры и таймауты
 unsigned long lastPacketTime = 0;
 const unsigned long SIGNAL_TIMEOUT_MS = 200; // Таймаут пропадания пакетов i-BUS
 
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3500; // Длительность фазы запуска двигателя
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; // Длительность сирены при потере связи
 
 unsigned long lastActivityTime = 0;
 const unsigned long AUTO_SHUTDOWN_MS = 30000; // Таймер простоя до автоотключения (30 сек)
 
 // Переменные для инерционного торможения и фикса громкости
 unsigned long shutdownStartTime = 0;
 int brakingStartThrottle = 1500;
 int brakingStartSteering = 1500;
 int lastThrottle = 1500;
 int lastSteering = 1500;
 int lastValidVrA = 2000; // Сохранение громкости (по умолчанию максимум)
 
 unsigned long lastDiagLogTime = 0; // Таймер для секундной телеметрии
 
 /**
  * @brief Проверка, находятся ли все основные стики в центральном положении.
  */
 bool isSticksCentered(int throttle, int steering, int turret) {
     return (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX) &&
            (steering > DEADBAND_MIN && steering < DEADBAND_MAX) &&
            (turret > DEADBAND_MIN && turret < DEADBAND_MAX);
 }
 
 void setup() {
   initMotors();
   initTurret();
   pinMode(PIN_SERVO, OUTPUT);
   digitalWrite(PIN_SERVO, LOW);
 
   Serial.begin(115200);
   initIBus(PIN_IBUS_RX);
   initAudio(); 
 
   delay(200);
   Serial.println("\n================================================");
   Serial.println("SYSTEM READY [v1.0.1]: FSM & AUDIO CACHE ACTIVE.");
   Serial.println("================================================\n");
 }
 
 void loop() {
   // Чтение шины i-BUS. Если получен пакет, обновляем время последней связи.
   bool hasValidPacket = readIBus();
   if (hasValidPacket) {
     lastPacketTime = millis();
   }
 
   // Извлечение значений управляющих каналов из глобального массива
   int throttle  = channels[1]; // Канал 2: Газ (танковый микшер)
   int steering  = channels[0]; // Канал 1: Руль / Поворот
   int turretRaw = channels[3]; // Канал 4: Управление башней
   int ch3       = channels[2]; // Канал 3: Газ/Связь (Failsafe)
   int vrA       = channels[4]; // Канал 5: Крутилка громкости
   int swa       = channels[6]; // Канал 7: Тумблер предохранителя SWA
 
   // Определение системных флагов безопасности
   bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
   bool ch3Failsafe = (ch3 <= 1000); 
   bool signalLost  = rxTimeout || ch3Failsafe; // Потеря связи (таймаут или аппаратный failsafe)
   bool isSwaArmed  = (swa > 1500);             // Тумблер SWA включен (боевой режим)
   bool isCentered  = isSticksCentered(throttle, steering, turretRaw);
 
   // --- ФИКС ГРОМКОСТИ (v1.0.1) ---
   // Кэшируем громкость только если связь стабильна. Это предотвращает
   // сброс громкости в 0 или 50%, когда приемник уходит в аппаратный Failsafe.
   if (!signalLost) {
       lastValidVrA = vrA;
   }
 
   // Применяем закэшированную (или актуальную) громкость
   if (lastValidVrA < 1050) setAudioVolume(0.0f);
   else setAudioVolume((lastValidVrA - 1050) / 950.0f);
   // -------------------------------
 
   // Первичная инициализация состояния при подаче питания или ресете МК
   if (isFirstLoop) {
       if (signalLost) currentState = STATE_DISCONNECTED;
       else if (!isSwaArmed) currentState = STATE_DISARMED;
       else currentState = STATE_STARTFAIL;
       previousState = currentState;
       warningStartTime = millis();
       isFirstLoop = false;
   }
 
   // Логирование моментов смены состояния системы
   if (currentState != previousState) {
       Serial.print("\n>>> STATE TRANSITION: ");
       Serial.print(stateNames[previousState]);
       Serial.print(" -> ");
       Serial.println(stateNames[currentState]);
       previousState = currentState;
   }
 
   // Периодическая секундная телеметрия для отладки
   if (millis() - lastDiagLogTime >= 1000) {
       lastDiagLogTime = millis();
       Serial.printf("[DIAG] State:%s | signalLost:%d | SWA:%d | Centered:%d\n",
                     stateNames[currentState], signalLost, swa, isCentered);
   }
 
   // --- ГЛАВНАЯ МАШИНА СОСТОЯНИЙ (FSM) ---
   switch (currentState) {
 
     case STATE_DISCONNECTED:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       // Сирена работает первые 15 секунд после обрыва связи, затем тишина
       if (millis() - warningStartTime < SIREN_MAX_DURATION_MS) setAudioMode(AUDIO_MODE_SIREN);
       else setAudioMode(AUDIO_MODE_MUTE);
       
       // Переход при восстановлении связи
       if (!signalLost) {
           if (!isSwaArmed) currentState = STATE_DISARMED;
           else currentState = STATE_STARTFAIL;
       }
       break;
 
     case STATE_DISARMED:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       setAudioMode(AUDIO_MODE_MUTE);
 
       if (signalLost) {
           currentState = STATE_DISCONNECTED;
           warningStartTime = millis();
       } else if (isSwaArmed) {
           // Если тумблер включили: проверяем стики. Если в центре — запуск, иначе ошибка.
           if (isCentered) {
               currentState = STATE_START;
               startTimer = millis();
               setAudioMode(AUDIO_MODE_START);
           } else {
               currentState = STATE_STARTFAIL;
           }
       }
       break;
 
     case STATE_STARTFAIL:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       setAudioMode(AUDIO_MODE_SIREN); // Ошибка оператора сопровождается сиреной
 
       if (signalLost) {
           currentState = STATE_DISCONNECTED;
           warningStartTime = millis();
       } else if (!isSwaArmed) {
           currentState = STATE_DISARMED;
       } else if (isCentered) {
           // Когда оператор вернул стики в центр — разрешаем запуск
           currentState = STATE_START;
           startTimer = millis();
           setAudioMode(AUDIO_MODE_START);
       }
       break;
 
     case STATE_START:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       
       // Экстренный уход в shutdown при обрыве связи или выключении SWA во время запуска
       if (signalLost || !isSwaArmed) {
           currentState = STATE_SHUTDOWN;
           shutdownStartTime = millis();
           brakingStartThrottle = 1500;
           brakingStartSteering = 1500;
           setAudioMode(AUDIO_MODE_STOP);
       } else if (millis() - startTimer >= START_DURATION_MS) {
           // По истечении времени стартового файла переходим в боевой режим
           currentState = STATE_RUNNING;
           lastActivityTime = millis();
           setAudioMode(AUDIO_MODE_ENGINE); 
       }
       break;
 
     case STATE_RUNNING:
       // КРИТИЧЕСКИ ВАЖНО: Проверка потери связи ДО применения стиков (защита от мусора при Failsafe OFF)
       if (signalLost || !isSwaArmed || (millis() - lastActivityTime > AUTO_SHUTDOWN_MS)) {
           currentState = STATE_SHUTDOWN;
           shutdownStartTime = millis();
           
           // Фиксируем последние ВАЛИДНЫЕ значения стиков перед разрывом связи для плавного тормоза
           brakingStartThrottle = lastThrottle;
           brakingStartSteering = lastSteering;
           
           setAudioMode(AUDIO_MODE_STOP);
           Serial.println("[DEBUG] SHUTDOWN TRIGGERED! Initiating safe 2-second brake...");
           break; // Немедленно выходим, исключая обработку нулевых данных приемника
       }
 
       // Сохраняем валидные данные стиков текущего цикла
       lastThrottle = throttle;
       lastSteering = steering;
       updateMixer(throttle, steering);
       
       // Сброс таймера автоотключения при любом движении стиков
       if (!isCentered) lastActivityTime = millis(); 
       
       // Управление башней
       if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) turretRaw = CENTER_VAL;
       setTurretSpeed(map(turretRaw, 1000, 2000, -255, 255));
       updateEngineSound(throttle);
       break;
 
     case STATE_SHUTDOWN:
       unsigned long shutdownElapsed = millis() - shutdownStartTime;
       
       // ФАЗА 1: Плавное инерционное торможение в течение первых 2 секунд (2000 мс)
       if (shutdownElapsed < 2000) {
           float progress = (float)shutdownElapsed / 2000.0f;
           // Математическое сведение виртуальных стиков из текущей позиции обратно в центр (1500)
           int currT = brakingStartThrottle + (1500 - brakingStartThrottle) * progress;
           int currS = brakingStartSteering + (1500 - brakingStartSteering) * progress;
           updateMixer(currT, currS);
           setTurretSpeed(0);
       } 
       // ФАЗА 2: Жесткое удержание моторов в нуле в течение последующих 3 секунд (до 5000 мс)
       else if (shutdownElapsed < 5000) {
           setMotorSpeeds(0, 0);
           setTurretSpeed(0);
       } 
       // ФАЗА 3: Снятие блокировки и разрешенные переходы
       else {
           setMotorSpeeds(0, 0);
           setTurretSpeed(0);
           
           if (signalLost) {
               currentState = STATE_DISCONNECTED;
               warningStartTime = millis();
           } else if (!isSwaArmed) {
               currentState = STATE_DISARMED;
           }
           // Если связь есть, но тумблер SWA всё еще физически поднят — висим в глухой блокировке SHUTDOWN.
       }
       break;
   }
 }
```

---

## File: .\src\mixer.cpp
```cpp
/**
 * @file mixer.cpp
 * @version 0.3
 * @brief Реализация танкового микшера.
 */

 #include "mixer.h"
 #include "motor_control.h"
 
 // Константы мертвой зоны стиков (чтобы танк не "полз", если стик чуть-чуть сбит)
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 void updateMixer(int throttle, int steering) {
     // 1. Применяем мертвую зону (отсекаем дрожание стиков в центре)
     if (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX) throttle = CENTER_VAL;
     if (steering > DEADBAND_MIN && steering < DEADBAND_MAX) steering = CENTER_VAL;
 
     // 2. Преобразуем микросекунды (1000...2000) в диапазон ШИМ мощности (-255...+255)
     int y = map(throttle, 1000, 2000, -255, 255);
     int x = map(steering, 1000, 2000, -255, 255);
 
     // 3. Классическая математика дифференциального привода (Танковый микшер)
     int leftSpeed = y + x;
     int rightSpeed = y - x;
 
     // 4. Жестко ограничиваем итоговые значения пределами 8 бит, 
     // чтобы при крайних диагональных положениях стика не было переполнения
     leftSpeed = constrain(leftSpeed, -255, 255);
     rightSpeed = constrain(rightSpeed, -255, 255);
 
     // 5. Отправляем рассчитанные скорости в модуль управления моторами
     setMotorSpeeds(leftSpeed, rightSpeed);
 }
```

---

## File: .\src\mixer.h
```cpp
/**
 * @file mixer.h
 * @version 0.3
 * @brief Модуль дифференциального микширования для гусеничной техники.
 * Переводит сигналы пульта в скорости независимых моторов.
 */

 #pragma once
 #include <Arduino.h>
 
 /**
  * @brief Расчет и передача скоростей на драйверы моторов.
  * @param throttle Значение канала газа (1000 - 2000, нейтраль 1500).
  * @param steering Значение канала поворота (1000 - 2000, нейтраль 1500).
  */
 void updateMixer(int throttle, int steering);
```

---

## File: .\src\motor_control.cpp
```cpp
/**
 * @file motor_control.cpp
 * @version 0.2
 * @brief Реализация управления ходовой частью с использованием подсистемы LEDC.
 */

 #include "motor_control.h"

 // --- ПИНЫ УПРАВЛЕНИЯ ---
 const int PIN_L_PWM_FWD = 16;
 const int PIN_L_PWM_REV = 17;
 const int PIN_R_PWM_FWD = 19;
 const int PIN_R_PWM_REV = 23;
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 // ESP32 имеет 16 независимых каналов генерации ШИМ. Выделяем первые 4 для ходовой.
 const int PWM_CH_L_FWD = 0;
 const int PWM_CH_L_REV = 1;
 const int PWM_CH_R_FWD = 2;
 const int PWM_CH_R_REV = 3;
 
 // Частота 20 кГц гарантирует отсутствие "писка" в обмотках моторов
 const int PWM_FREQ = 20000; 
 // 8 бит дают нам шкалу мощности от 0 до 255
 const int PWM_RES = 8;      
 
 void initMotors() {
     // 1. Настройка частоты и разрешения для выделенных каналов
     ledcSetup(PWM_CH_L_FWD, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_L_REV, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_R_FWD, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_R_REV, PWM_FREQ, PWM_RES);
 
     // 2. Аппаратная привязка физических пинов к каналам генератора
     ledcAttachPin(PIN_L_PWM_FWD, PWM_CH_L_FWD);
     ledcAttachPin(PIN_L_PWM_REV, PWM_CH_L_REV);
     ledcAttachPin(PIN_R_PWM_FWD, PWM_CH_R_FWD);
     ledcAttachPin(PIN_R_PWM_REV, PWM_CH_R_REV);
 
     // 3. Блокировка: принудительная подача 0 на все каналы
     setMotorSpeeds(0, 0);
 }
 
 void setMotorSpeeds(int leftSpeed, int rightSpeed) {
     // Жесткое ограничение входных значений математическими рамками 8 бит
     leftSpeed = constrain(leftSpeed, -255, 255);
     rightSpeed = constrain(rightSpeed, -255, 255);
 
     // Логика переключения левой гусеницы
     if (leftSpeed > 0) {
         ledcWrite(PWM_CH_L_FWD, leftSpeed);
         ledcWrite(PWM_CH_L_REV, 0);
     } else if (leftSpeed < 0) {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, -leftSpeed); // Берем значение по модулю
     } else {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, 0);
     }
 
     // Логика переключения правой гусеницы
     if (rightSpeed > 0) {
         ledcWrite(PWM_CH_R_FWD, rightSpeed);
         ledcWrite(PWM_CH_R_REV, 0);
     } else if (rightSpeed < 0) {
         ledcWrite(PWM_CH_R_FWD, 0);
         ledcWrite(PWM_CH_R_REV, -rightSpeed);
     } else {
         ledcWrite(PWM_CH_R_FWD, 0);
         ledcWrite(PWM_CH_R_REV, 0);
     }
 }
```

---

## File: .\src\motor_control.h
```cpp
/**
 * @file motor_control.h
 * @version 0.2
 * @brief Модуль управления ходовыми двигателями танка (драйверы BTS7960).
 * Использует аппаратный ШИМ (LEDC) ESP32.
 */

 #pragma once
 #include <Arduino.h>
 
 /**
  * @brief Инициализация ШИМ-каналов и привязка пинов. 
  * Устанавливает начальное безопасное состояние (остановка).
  */
 void initMotors();
 
 /**
  * @brief Установка скорости и направления вращения гусениц.
  * @param leftSpeed Скорость левой гусеницы (от -255 до 255). 
  * Положительное значение — вперед, отрицательное — назад.
  * @param rightSpeed Скорость правой гусеницы (от -255 до 255).
  */
 void setMotorSpeeds(int leftSpeed, int rightSpeed);
```

---

## File: .\src\turret_control.cpp
```cpp
/**
 * @file turret_control.cpp
 * @version 0.4
 * @brief Реализация управления приводом башни с использованием подсистемы LEDC.
 */

 #include "turret_control.h"

 // --- ФИЗИЧЕСКИЕ ПИНЫ УПРАВЛЕНИЯ БАШНЕЙ ---
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 // Выделяем каналы 4 и 5 (каналы 0–3 заняты ходовой частью)
 const int PWM_CH_TURRET_IN1 = 4;
 const int PWM_CH_TURRET_IN2 = 5;
 
 // Частота 20 кГц (унифицирована с ходовой частью)
 const int PWM_FREQ = 20000; 
 // Разрешение 8 бит (шкала мощности 0..255)
 const int PWM_RES = 8;      
 
 void initTurret() {
     // 1. Настройка частоты и разрешения для выделенных каналов ШИМ
     ledcSetup(PWM_CH_TURRET_IN1, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_TURRET_IN2, PWM_FREQ, PWM_RES);
 
     // 2. Привязка физических пинов к каналам генератора
     ledcAttachPin(PIN_TURRET_IN1, PWM_CH_TURRET_IN1);
     ledcAttachPin(PIN_TURRET_IN2, PWM_CH_TURRET_IN2);
 
     // 3. Блокировка: начальное состояние — остановка
     setTurretSpeed(0);
 }
 
 void setTurretSpeed(int speed) {
     // Ограничение входного значения в пределах 8 бит
     speed = constrain(speed, -255, 255);
 
     if (speed > 0) {
         ledcWrite(PWM_CH_TURRET_IN1, speed);
         ledcWrite(PWM_CH_TURRET_IN2, 0);
     } else if (speed < 0) {
         ledcWrite(PWM_CH_TURRET_IN1, 0);
         ledcWrite(PWM_CH_TURRET_IN2, -speed);
     } else {
         ledcWrite(PWM_CH_TURRET_IN1, 0);
         ledcWrite(PWM_CH_TURRET_IN2, 0);
     }
 }
```

---

## File: .\src\turret_control.h
```cpp
/**
 * @file turret_control.h
 * @version 0.4
 * @brief Модуль управления приводом вращения башни (драйвер DRV8833).
 * Использует аппаратный ШИМ (LEDC) ESP32.
 */

 #pragma once
 #include <Arduino.h>
 
 /**
  * @brief Инициализация ШИМ-каналов и привязка пинов управления башней.
  * Устанавливает начальное безопасное состояние (остановка).
  */
 void initTurret();
 
 /**
  * @brief Установка скорости и направления вращения башни.
  * @param speed Скорость вращения (от -255 до 255).
  * Положительное значение — вращение в одну сторону, отрицательное — в противоположную.
  */
 void setTurretSpeed(int speed);
```

---

