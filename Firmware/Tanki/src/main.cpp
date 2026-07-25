/**
 * @file main.cpp
 * @version 0.10
 * @brief Главный файл прошивки радиоуправляемого танка 1:16.
 * * Внедрена архитектура Конечного Автомата (FSM).
 * * Канал 7 (SWA) используется как аппаратный предохранитель.
 * * Реализована 3-секундная фаза запуска двигателя с блокировкой ввода.
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
 
 // Состояния системы (FSM)
 enum SystemState {
     STATE_DISARMED, // Танк выключен тумблером SWA (Тишина)
     STATE_WARNING,  // Сирена (Стики не в центре или нет связи)
     STATE_STARTING, // Запуск двигателя (Блокировка на 3 секунды)
     STATE_RUNNING   // Рабочий режим (Движение и звук мотора)
 };
 
 SystemState currentState = STATE_DISARMED;
 
 // Таймеры 
 unsigned long lastPacketTime = 0;
 const unsigned long SIGNAL_TIMEOUT_MS = 200; 
 
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3000; // 3 секунды на звук стартера
 
 /**
  * @brief Проверяет, находятся ли все стики в нейтральном положении
  */
 bool isSticksCentered(int throttle, int steering, int turret) {
     return (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX) &&
            (steering > DEADBAND_MIN && steering < DEADBAND_MAX) &&
            (turret > DEADBAND_MIN && turret < DEADBAND_MAX);
 }
 
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
   Serial.println("SYSTEM READY [v0.10]: FSM & Safe Start Architecture.");
   Serial.println("Turn Switch SWA (Ch7) to ON to initiate starting sequence.");
   Serial.println("================================================\n");
 }
 
 void loop() {
   if (readIBus()) {
     lastPacketTime = millis();
   }
 
   // Сбор текущих показаний пульта
   int throttle = channels[1];
   int steering = channels[0];
   int turretRaw = channels[3];
   int swa = channels[6]; // Канал 7 (Индекс 6 в массиве)
 
   // Флаги прерываний
   bool signalLost = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
   bool isSwaArmed = (swa > 1500); // Положение 2 (2000)
 
   // ---------------------------------------------------------
   // 1. АППАРАТНЫЕ ПЕРЕХВАТЫ (Высший приоритет)
   // ---------------------------------------------------------
   if (signalLost) {
     // Обрыв связи: немедленный переход в предупреждение
     currentState = STATE_WARNING;
   } else if (!isSwaArmed) {
     // Тумблер SWA в положении 1: мгновенная смерть
     currentState = STATE_DISARMED;
   }
 
   // ---------------------------------------------------------
   // 2. ОБРАБОТКА ТЕКУЩЕГО СОСТОЯНИЯ (FSM)
   // ---------------------------------------------------------
   switch (currentState) {
       
     case STATE_DISARMED:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       setAudioMode(AUDIO_MODE_MUTE);
 
       // Переход к старту, если включили тумблер
       if (isSwaArmed && !signalLost) {
         if (isSticksCentered(throttle, steering, turretRaw)) {
           currentState = STATE_STARTING;
           startTimer = millis();
           setAudioMode(AUDIO_MODE_START);
           Serial.println("STARTING ENGINE...");
         } else {
           currentState = STATE_WARNING;
         }
       }
       break;
 
     case STATE_WARNING:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       setAudioMode(AUDIO_MODE_SIREN);
 
       // Переход к старту, если вернули стики в центр
       if (isSwaArmed && !signalLost) {
         if (isSticksCentered(throttle, steering, turretRaw)) {
           currentState = STATE_STARTING;
           startTimer = millis();
           setAudioMode(AUDIO_MODE_START);
           Serial.println("STARTING ENGINE...");
         }
       }
       break;
 
     case STATE_STARTING:
       // В этом режиме гусеницы обесточены, а стики игнорируются!
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       
       // Ожидание завершения звука start.wav (3 секунды)
       if (millis() - startTimer >= START_DURATION_MS) {
         currentState = STATE_RUNNING;
         setAudioMode(AUDIO_MODE_ENGINE); // Переключаем на холостой ход
         Serial.println("ENGINE RUNNING. ARMED!");
       }
       break;
 
     case STATE_RUNNING:
       // Штатное управление гусеницами
       updateMixer(throttle, steering);
       
       // Штатное управление башней
       if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) {
         turretRaw = CENTER_VAL;
       }
       int turretSpeed = map(turretRaw, 1000, 2000, -255, 255);
       setTurretSpeed(turretSpeed);
       
       // Динамический звук мотора
       updateEngineSound(throttle);
       break;
   }
 }