/**
 * @file main.cpp
 * @version 0.2.1
 * @brief Главный диспетчер задач.
 * * Диагностический шаг 2.1: Проверка ходовой части прямой цифровой логикой (HIGH/LOW).
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 
 // Заблокированная периферия башни и орудия
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 const int PIN_SERVO = 14;
 
 // Пин аппаратного UART для i-BUS
 const int PIN_IBUS_RX = 34;
 
 void setup() {
   // Инициализация ходовых пинов в режиме GPIO OUTPUT
   initMotors();
 
   // Аппаратный Safety Lock для башни и сервопривода
   pinMode(PIN_TURRET_IN1, OUTPUT);
   pinMode(PIN_TURRET_IN2, OUTPUT);
   pinMode(PIN_SERVO, OUTPUT);
 
   digitalWrite(PIN_TURRET_IN1, LOW);
   digitalWrite(PIN_TURRET_IN2, LOW);
   digitalWrite(PIN_SERVO, LOW);
 
   // Инициализация портов связи
   Serial.begin(115200);
   initIBus(PIN_IBUS_RX);
 
   delay(200);
   Serial.println("\n================================================");
   Serial.println("DIAGNOSTIC MODE ACTIVE [v0.2.1]: Direct GPIO Write.");
   Serial.println("Testing raw 3.3V logic signals on motor pins.");
   Serial.println("================================================\n");
 }
 
 void loop() {
   static unsigned long lastPrintTime = 0;
   static unsigned long lastTestTime = 0;
   static int testState = 0;
 
   // Полноценный опрос i-BUS (для контроля параллельной работы систем)
   if (readIBus()) {
     unsigned long currentTime = millis();
     if (currentTime - lastPrintTime >= 100) {
       lastPrintTime = currentTime;
       Serial.printf("CH1: %4d | CH2: %4d | CH3: %4d | CH4: %4d\n", 
                     channels[0], channels[1], channels[2], channels[3]);
     }
   }
 
   // Неблокирующий автомат переключения прямых логических уровней
   unsigned long currentTime = millis();
   if (currentTime - lastTestTime >= 3000) {
     lastTestTime = currentTime;
     testState = (testState + 1) % 4;
 
     switch (testState) {
       case 0:
         Serial.println(">>> ТЕСТ: Остановка моторов (0V)");
         setMotorSpeeds(0, 0);
         break;
       case 1:
         Serial.println(">>> ТЕСТ: Полный вперед (Постоянный HIGH)");
         setMotorSpeeds(255, 255);
         break;
       case 2:
         Serial.println(">>> ТЕСТ: Остановка моторов (0V)");
         setMotorSpeeds(0, 0);
         break;
       case 3:
         Serial.println(">>> ТЕСТ: Полный назад (Постоянный HIGH в реверс)");
         setMotorSpeeds(-255, -255);
         break;
     }
   }
 }