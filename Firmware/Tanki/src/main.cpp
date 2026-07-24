/**
 * @file main.cpp
 * @version 0.9
 * @brief Главный файл прошивки радиоуправляемого танка 1:16.
 * * Внедрена функция Safe Start (Арминг).
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
   Serial.println("SYSTEM READY [v0.9]: SAFE START ENABLED.");
   Serial.println("Waiting for sticks to be centered...");
   Serial.println("================================================\n");
 }
 
 void loop() {
   if (readIBus()) {
     int throttle = channels[1];
     int steering = channels[0];
     int turretRaw = channels[3];
 
     // Проверка положения стиков (находятся ли они в центре)
     bool isThrottleCentered = (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX);
     bool isSteeringCentered = (steering > DEADBAND_MIN && steering < DEADBAND_MAX);
     bool isTurretCentered = (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX);
 
     if (!isSystemArmed) {
       // СОСТОЯНИЕ БЛОКИРОВКИ
       if (isThrottleCentered && isSteeringCentered && isTurretCentered) {
         // Успешный Арминг: все стики в центре
         isSystemArmed = true;
         setAudioMode(AUDIO_MODE_ENGINE); // Запуск звука двигателя
         Serial.println("ARMED! System is ready to move.");
       } else {
         // Стики сдвинуты: блокируем моторы и включаем сирену
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