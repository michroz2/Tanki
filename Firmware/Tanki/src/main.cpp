/**
 * @file main.cpp
 * @version 0.8
 * @brief Главный файл прошивки радиоуправляемого танка 1:16.
 * * Интегрирована связь скорости вращения моторов с тональностью 
 * аудио-двигателя (вызов updateEngineSound).
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
   Serial.println("SYSTEM READY [v0.8]: Dynamic RPM Engine Audio.");
   Serial.println("================================================\n");
 }
 
 void loop() {
   if (readIBus()) {
     int throttle = channels[1];
     int steering = channels[0];
     
     // Обновляем микшер моторов
     updateMixer(throttle, steering);
     
     // ОБНОВЛЕНИЕ: Передаем текущий газ в аудио-подсистему
     updateEngineSound(throttle);
 
     int turretRaw = channels[3];
     if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) {
       turretRaw = CENTER_VAL;
     }
     int turretSpeed = map(turretRaw, 1000, 2000, -255, 255);
     setTurretSpeed(turretSpeed);
   }
 }