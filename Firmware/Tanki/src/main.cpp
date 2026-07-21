/**
 * @file main.cpp
 * @version 0.1
 * @brief Главный диспетчер задач и инициализации систем танка.
 * * Архитектурный шаг 1: Вынесение логики i-BUS в отдельный модуль.
 * Безопасность ходовой части и башни полностью сохранена.
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 
 // --- НАЗНАЧЕНИЕ ФИЗИЧЕСКИХ ПИНОВ ПРОЦЕССОРА ---
 // Левый силовой драйвер BTS7960 (Ходовая часть)
 const int PIN_L_PWM_FWD = 16; 
 const int PIN_L_PWM_REV = 17;
 
 // Правый силовой драйвер BTS7960 (Ходовая часть)
 const int PIN_R_PWM_FWD = 19; 
 const int PIN_R_PWM_REV = 23;
 
 // Драйвер DRV8833 (Привод вращения башни)
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 
 // Пин управления сервоприводом подъема орудия MG90S
 const int PIN_SERVO = 14;
 
 // Выделенный пин аппаратного UART для чтения телеметрии i-BUS
 const int PIN_IBUS_RX = 34;
 
 void setup() {
   // 1. Аппаратный Safety Lock: Перевод всех выходов силовых модулей в безопасное состояние LOW
   pinMode(PIN_L_PWM_FWD, OUTPUT);
   pinMode(PIN_L_PWM_REV, OUTPUT);
   pinMode(PIN_R_PWM_FWD, OUTPUT);
   pinMode(PIN_R_PWM_REV, OUTPUT);
   pinMode(PIN_TURRET_IN1, OUTPUT);
   pinMode(PIN_TURRET_IN2, OUTPUT);
   pinMode(PIN_SERVO, OUTPUT);
 
   digitalWrite(PIN_L_PWM_FWD, LOW);
   digitalWrite(PIN_L_PWM_REV, LOW);
   digitalWrite(PIN_R_PWM_FWD, LOW);
   digitalWrite(PIN_R_PWM_REV, LOW);
   digitalWrite(PIN_TURRET_IN1, LOW);
   digitalWrite(PIN_TURRET_IN2, LOW);
   digitalWrite(PIN_SERVO, LOW);
 
   // 2. Инициализация диагностического последовательного порта для связи с компьютером через USB
   Serial.begin(115200);
 
   // 3. Запуск изолированного модуля чтения i-BUS
   initIBus(PIN_IBUS_RX);
 
   delay(200);
   Serial.println("\n================================================");
   Serial.println("SAFE CORE ACTIVE [v0.1]: i-BUS Module Isolated.");
   Serial.println("All motors locked. Data routing check ready.");
   Serial.println("================================================\n");
 }
 
 void loop() {
   static unsigned long lastPrintTime = 0;
 
   // Опрос изолированного аппаратного модуля парсинга данных
   if (readIBus()) {
     unsigned long currentTime = millis();
     
     // Вывод телеметрии первых четырех каналов пульта в консоль раз в 100 мс
     if (currentTime - lastPrintTime >= 100) {
       lastPrintTime = currentTime;
 
       Serial.printf("CH1: %4d | CH2: %4d | CH3: %4d | CH4: %4d\n", 
                     channels[0], channels[1], channels[2], channels[3]);
     }
   }
 }