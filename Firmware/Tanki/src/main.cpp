/**
 * @file main.cpp
 * @version 0.6
 * @brief Главный файл прошивки радиоуправляемого танка 1:16.
 * * Точка входа в программу. Инициализирует все подсистемы:
 * моторы, башню, чтение пульта i-BUS и аудио-ядро I2S.
 * Основной цикл (loop) работает на Ядре 1 и отвечает за парсинг и микширование.
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 #include "turret_control.h"
 #include "audio_system.h" // Подключение подсистемы аудио

 // Пин управления сервоприводом подъема орудия MG90S (заблокирован до Шага 5)
 const int PIN_SERVO = 14;
 
 // Пин аппаратного UART для i-BUS
 const int PIN_IBUS_RX = 34;
 
 // Константы мертвой зоны для канала башни
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 void setup() {
   // 1. Инициализация подсистем моторов (ШИМ 20 кГц)
   initMotors();
   initTurret();
 
   // 2. Аппаратный Safety Lock для сервопривода орудия
   pinMode(PIN_SERVO, OUTPUT);
   digitalWrite(PIN_SERVO, LOW);
 
   // 3. Инициализация портов связи
   Serial.begin(115200);
   initIBus(PIN_IBUS_RX);

  // Инициализация и запуск I2S аудио на Ядре 0
   initAudio(); // <--- ДОБАВЛЕНО: Запуск аудио-ядра
   
   delay(200);
   Serial.println("\n================================================");
   Serial.println("SYSTEM READY [v0.6]: Turret Control & I2S Audio Active.");
   Serial.println("Waiting for FlySky RC input...");
   Serial.println("================================================\n");
 }
 
 void loop() {
   static unsigned long lastPrintTime = 0;
 
   // Опрос приемника i-BUS
   if (readIBus()) {
     // --- ХОДОВАЯ ЧАСТЬ ---
     // CH2 (индекс 1) — Газ (Вперед/Назад)
     // CH1 (индекс 0) — Руль (Влево/Вправо)
     int throttle = channels[1];
     int steering = channels[0];
     updateMixer(throttle, steering);
 
     // --- УПРАВЛЕНИЕ БАШНЕЙ ---
     // CH4 (индекс 3) — Левый стик по горизонтали (Вращение башни)
     int turretRaw = channels[3];
     
     // Применение мертвой зоны
     if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) {
       turretRaw = CENTER_VAL;
     }
 
     // Преобразование диапазона 1000..2000 мкс в ШИМ -255..255
     int turretSpeed = map(turretRaw, 1000, 2000, -255, 255);
     setTurretSpeed(turretSpeed);
 
     // Вывод телеметрии для визуального контроля
     unsigned long currentTime = millis();
     if (currentTime - lastPrintTime >= 200) {
       lastPrintTime = currentTime;
       Serial.printf("Throttle: %4d | Steering: %4d | Turret CH4: %4d -> Speed: %4d\n", 
                     throttle, steering, channels[3], turretSpeed);
     }
   }
 }