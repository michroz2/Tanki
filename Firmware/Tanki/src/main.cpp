/**
 * @file main.cpp
 * @version 0.3
 * @brief Главный диспетчер задач.
 * * Архитектурный шаг 3: Внедрение танкового микшера (управление с пульта).
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 
 // Заблокированная периферия башни и орудия
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 const int PIN_SERVO = 14;
 
 // Пин аппаратного UART для i-BUS
 const int PIN_IBUS_RX = 34;
 
 void setup() {
   // 1. Инициализация ходовой части (ШИМ 20 кГц)
   initMotors();
 
   // 2. Аппаратный Safety Lock для башни и сервопривода
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
   Serial.println("SYSTEM READY [v0.3]: Tank Mixer Active.");
   Serial.println("Waiting for FlySky RC input...");
   Serial.println("================================================\n");
 }
 
 void loop() {
   static unsigned long lastPrintTime = 0;
 
   // Опрос приемника
   if (readIBus()) {
     // CH2 (индекс 1) - Вперед/Назад (Правый стик по вертикали)
     // CH1 (индекс 0) - Влево/Вправо (Правый стик по горизонтали)
     int throttle = channels[1];
     int steering = channels[0];
 
     // Передаем данные со стиков в микшер, который сам крутит моторы
     updateMixer(throttle, steering);
 
     // Вывод телеметрии для визуального контроля (5 раз в секунду)
     unsigned long currentTime = millis();
     if (currentTime - lastPrintTime >= 200) {
       lastPrintTime = currentTime;
       Serial.printf("Throttle: %4d | Steering: %4d\n", throttle, steering);
     }
   } else {
     // ВАЖНО: Если связь с пультом потеряна (приемник не шлет пакеты),
     // мы должны немедленно остановить танк! (Failsafe)
     // Данную логику можно будет расширить, но пока защита базируется на том,
     // что приемник FS-iA6B при потере связи сам выдает 1500 по всем каналам.
   }
 }