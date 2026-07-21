/**
 * @file ibus_parser.cpp
 * @version 0.1
 * @brief Реализация парсера протокола i-BUS с аппаратным контролем целостности данных.
 */

 #include "ibus_parser.h"

 // Инициализация глобального массива каналов нейтральными значениями (1500 мкс — центр стиков)
 int channels[IBUS_MAX_CHANNELS] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
 
 // Внутренний статический буфер для накопления сырых байт пакета (размер пакета всегда 32 байта)
 static uint8_t ibusBuffer[32];
 
 void initIBus(int rxPin) {
     // Настройка аппаратного порта Serial2: скорость 115200 бод, 8 бит данных, без четности, 1 стоп-бит.
     // Пин передачи (TX) устанавливается в -1, так как шина i-BUS на приемнике работает исключительно на выход.
     Serial2.begin(115200, SERIAL_8N1, rxPin, -1);
 }
 
 bool readIBus() {
     while (Serial2.available()) {
         uint8_t val = Serial2.read();
         static int byteIdx = 0;
 
         // Поиск маркера начала пакета i-BUS: 1-й байт равен 0x20 (длина), 2-й байт равен 0x40 (тип команды)
         if (byteIdx == 0 && val == 0x20) {
             ibusBuffer[0] = val;
             byteIdx = 1;
         } else if (byteIdx == 1 && val == 0x40) {
             ibusBuffer[1] = val;
             byteIdx = 2;
         } else if (byteIdx >= 2) {
             // Накопление байт данных в буфер
             ibusBuffer[byteIdx] = val;
             byteIdx++;
 
             // Пакет полностью собран (достиг 32 байт), переходим к валидации
             if (byteIdx >= 32) {
                 byteIdx = 0; // Сброс индекса для подготовки к приему следующего пакета
 
                 // Вычисление контрольной суммы (согласно спецификации протокола FlySky)
                 uint16_t checksum = 0xFFFF;
                 for (int i = 0; i < 30; i++) {
                     checksum -= ibusBuffer[i];
                 }
 
                 // Чтение контрольной суммы, пришедшей в пакете (Little-Endian: младший байт первый)
                 uint16_t rxChecksum = ibusBuffer[30] | (ibusBuffer[31] << 8);
 
                 // Если данные не повреждены — обновляем глобальный массив каналов
                 if (checksum == rxChecksum) {
                     for (int i = 0; i < IBUS_MAX_CHANNELS; i++) {
                         // Каждые 2 байта данных, начиная с индексов 2 и 3, формируют значение одного канала
                         channels[i] = ibusBuffer[2 + i * 2] | (ibusBuffer[3 + i * 2] << 8);
                     }
                     return true;
                 }
             }
         } else {
             // В случае сбоя синхронизации сбрасываем указатель пакета
             byteIdx = 0;
         }
     }
     return false;
 }
 