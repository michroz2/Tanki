/**
 * @file main.cpp
 * @version 0.2
 * @brief Главный диспетчер задач.
 * * Архитектурный шаг 2: Изоляция ШИМ-подсистемы хода и добавление тестового автомата.
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 
 // Драйвер DRV8833 (Привод вращения башни)
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 
 // Пин управления сервоприводом подъема орудия MG90S
 const int PIN_SERVO = 14;
 
 // Пин аппаратного UART для i-BUS
 const int PIN_IBUS_RX = 34;
 
 void setup() {
   // 1. Инициализация ходовой части (ШИМ 20 кГц) и безопасный старт (0)
   initMotors();
 
   // 2. Аппаратный Safety Lock: блокировка башни и орудия
   pinMode(PIN_TURRET_IN1, OUTPUT);
   pinMode(PIN_TURRET_IN2, OUTPUT);
   pinMode(PIN_SERVO, OUTPUT);
 
   digitalWrite(PIN_TURRET_IN1, LOW);
   digitalWrite(PIN_TURRET_IN2, LOW);
   digitalWrite(PIN_SERVO, LOW);
 
   // 3. Инициализация портов связи
   Serial.begin(115200);
   initIBus(PIN_IBUS_RX);
 
   delay(200);
   Serial.println("\n================================================");
   Serial.println("SAFE CORE ACTIVE [v0.2]: Motor PWM Initialized.");
   Serial.println("Starting hardware test sequence (motors only).");
   Serial.println("================================================\n");
 }
 
 void loop() {
   static unsigned long lastPrintTime = 0;
   static unsigned long lastTestTime = 0;
   static int testState = 0;
 
   // Опрос изолированного аппаратного модуля i-BUS
   if (readIBus()) {
     unsigned long currentTime = millis();
     if (currentTime - lastPrintTime >= 100) {
       lastPrintTime = currentTime;
       // Вывод телеметрии сохраняется для проверки связи с пультом
       Serial.printf("CH1: %4d | CH2: %4d | CH3: %4d | CH4: %4d\n", 
                     channels[0], channels[1], channels[2], channels[3]);
     }
   }
 
   // Простой неблокирующий автомат тестирования ходовой части
   // Переключает состояние моторов каждые 3 секунды
   unsigned long currentTime = millis();
   if (currentTime - lastTestTime >= 3000) {
     lastTestTime = currentTime;
     testState = (testState + 1) % 4; // Переключение 0 -> 1 -> 2 -> 3 -> 0
 
     switch (testState) {
       case 0:
         Serial.println(">>> АВТОТЕСТ: Остановка моторов (0)");
         setMotorSpeeds(0, 0);
         break;
       case 1:
         Serial.println(">>> АВТОТЕСТ: Движение ВПЕРЕД (Мощность: 100/255)");
         setMotorSpeeds(100, 100);
         break;
       case 2:
         Serial.println(">>> АВТОТЕСТ: Остановка моторов (0)");
         setMotorSpeeds(0, 0);
         break;
       case 3:
         Serial.println(">>> АВТОТЕСТ: Движение НАЗАД (Мощность: -100/255)");
         setMotorSpeeds(-100, -100);
         break;
     }
   }
 }