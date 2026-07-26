/**
 * @file ibus_parser.cpp
 * @version 0.1-DIAG
 * @brief Реализация парсера с ловушкой бесконечного цикла UART.
 */

 #include "ibus_parser.h"

 int channels[IBUS_MAX_CHANNELS] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
 static uint8_t ibusBuffer[32];
 
 void initIBus(int rxPin) {
     Serial2.begin(115200, SERIAL_8N1, rxPin, -1);
 }
 
 bool readIBus() {
     int emergencyCounter = 0; // СЧЕТЧИК-ЛОВУШКА
 
     while (Serial2.available()) {
         emergencyCounter++;
 
         // Если прочитали больше 200 байт за раз — это баг драйвера UART
         if (emergencyCounter == 200) {
             Serial.println("\n>>> FATAL ERROR: UART DRIVER STUCK! <<<");
             Serial.println("Infinite loop detected in readIBus(). Breaking out!");
             
             // Пытаемся сбросить зависший аппаратный буфер
             Serial2.flush(); 
             
             // Спасаем процессор от вечного зависания
             break; 
         }
 
         uint8_t val = Serial2.read();
         static int byteIdx = 0;
 
         if (byteIdx == 0 && val == 0x20) {
             ibusBuffer[0] = val;
             byteIdx = 1;
         } else if (byteIdx == 1 && val == 0x40) {
             ibusBuffer[1] = val;
             byteIdx = 2;
         } else if (byteIdx >= 2) {
             ibusBuffer[byteIdx] = val;
             byteIdx++;
 
             if (byteIdx >= 32) {
                 byteIdx = 0;
                 uint16_t checksum = 0xFFFF;
                 for (int i = 0; i < 30; i++) {
                     checksum -= ibusBuffer[i];
                 }
 
                 uint16_t rxChecksum = ibusBuffer[30] | (ibusBuffer[31] << 8);
 
                 if (checksum == rxChecksum) {
                     for (int i = 0; i < IBUS_MAX_CHANNELS; i++) {
                         channels[i] = ibusBuffer[2 + i * 2] | (ibusBuffer[3 + i * 2] << 8);
                     }
                     return true;
                 }
             }
         } else {
             byteIdx = 0;
         }
     }
     return false;
 }