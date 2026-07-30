/**
 * @file ibus_parser.cpp
 * @version 1.1
 * @brief Реализация парсера протокола FlySky i-BUS с защитой от сбоев UART.
 * * v1.1: Использование глобальных констант безопасности из config.h
 */

 #include "ibus_parser.h"
 #include "config.h"
 
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
 
         // Защитная ловушка: принудительно прерываем цикл, чтобы не "повесить" процессор.
         if (emergencyCounter == EMERGENCY_COUNTER) {
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