/**
 * @file main.cpp
 * @version 0.9.1 (Hotfix)
 * @brief Главный файл прошивки радиоуправляемого танка 1:16.
 * * Внедрена функция Safe Start (Арминг) + Аппаратный Failsafe (таймаут).
 */

 #include <Arduino.h>
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 #include "turret_control.h"
 #include "audio_system.h" 
 
 const int PIN_SERVO = 14;
 const int PIN_IBUS_RX = 34;
 
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 // Глобальный флаг безопасности
 bool isSystemArmed = false; 
 
 // Таймер для отслеживания связи с пультом (Failsafe)
 unsigned long lastPacketTime = 0;
 const unsigned long SIGNAL_TIMEOUT_MS = 200; // Если сигнала нет 0.2 сек - тревога
 
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
   Serial.println("SYSTEM READY [v0.9.1]: SAFE START & FAILSAFE ENABLED.");
   Serial.println("Waiting for sticks to be centered or Radio to connect...");
   Serial.println("================================================\n");
 }
 
 void loop() {
   // 1. Читаем данные с приемника. 
   // Если пришел валидный пакет - обновляем таймер последней активности.
   if (readIBus()) {
     lastPacketTime = millis();
   }
 
   // 2. Логика принятия решений вынесена наружу, чтобы работать независимо от пакетов
   if (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS) {
     // СИГНАЛ ПОТЕРЯН ИЛИ ПУЛЬТ ВЫКЛЮЧЕН
     if (isSystemArmed) {
         Serial.println("WARNING: SIGNAL LOST! Disarming and stopping motors...");
     }
     isSystemArmed = false; // Принудительно сбрасываем арминг
     
     // Глушим моторы ради безопасности
     setMotorSpeeds(0, 0);
     setTurretSpeed(0);
     
     // Включаем тревожную сирену
     setAudioMode(AUDIO_MODE_SIREN);
     
   } else {
     // СИГНАЛ ЕСТЬ (Пульт включен и работает штатно)
     int throttle = channels[1];
     int steering = channels[0];
     int turretRaw = channels[3];
 
     // Проверка положения стиков (находятся ли они в центре)
     bool isThrottleCentered = (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX);
     bool isSteeringCentered = (steering > DEADBAND_MIN && steering < DEADBAND_MAX);
     bool isTurretCentered = (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX);
 
     if (!isSystemArmed) {
       // СОСТОЯНИЕ БЛОКИРОВКИ (Ожидание приведения стиков в центр)
       if (isThrottleCentered && isSteeringCentered && isTurretCentered) {
         isSystemArmed = true;
         setAudioMode(AUDIO_MODE_ENGINE); // Запуск звука двигателя
         Serial.println("ARMED! System is ready to move.");
       } else {
         // Стики сдвинуты: блокируем моторы и продолжаем сирену
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         setAudioMode(AUDIO_MODE_SIREN);
       }
     } else {
       // НОРМАЛЬНОЕ СОСТОЯНИЕ УПРАВЛЕНИЯ (после успешного арминга)
       updateMixer(throttle, steering);
       
       if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) {
         turretRaw = CENTER_VAL;
       }
       int turretSpeed = map(turretRaw, 1000, 2000, -255, 255);
       setTurretSpeed(turretSpeed);
       
       updateEngineSound(throttle);
     }
   }
 }