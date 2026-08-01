/**
 * @file main.cpp
 * @version 1.5
 * @brief Главный файл прошивки радиоуправляемого танка 1:16 (ESP32).
 * * v1.5: Интеграция модуля защиты АКБ (battery_monitor). Добавлено STATE_LOW_BATTERY.
 */

 #include <Arduino.h>
 #include "config.h"
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 #include "turret_control.h"
 #include "audio_system.h" 
 #include "battery_monitor.h"
 
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 enum SystemState {
     STATE_DISCONNECTED, 
     STATE_DISARMED,     
     STATE_STARTFAIL,    
     STATE_START,        
     STATE_RUNNING,      
     STATE_SHUTDOWN,
     STATE_LOW_BATTERY   // НОВОЕ СОСТОЯНИЕ: Критический разряд АКБ
 };
 
 SystemState currentState = STATE_DISCONNECTED;
 SystemState previousState = STATE_DISCONNECTED; 
 bool isFirstLoop = true;
 
 const char* stateNames[] = {"DISCONNECTED", "DISARMED", "STARTFAIL", "START", "RUNNING", "SHUTDOWN", "LOW_BATTERY"};
 
 unsigned long lastPacketTime = 0;
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3500; 
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; 
 
 unsigned long lastActivityTime = 0;
 const unsigned long AUTO_SHUTDOWN_MS = 30000; 
 
 unsigned long shutdownStartTime = 0;
 int lastValidVrA = 2000; 
 int lastValidVrB = 1000; 
 
 unsigned long lastDiagLogTime = 0; 
 
 bool isSticksCentered(int throttle, int steering, int turret) {
     return (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX) &&
            (steering > DEADBAND_MIN && steering < DEADBAND_MAX) &&
            (turret > DEADBAND_MIN && turret < DEADBAND_MAX);
 }
 
 void setup() {
     initMotors();
     initTurret();
     initBatteryMonitor(); // Инициализация АЦП
     pinMode(PIN_SERVO, OUTPUT);
     digitalWrite(PIN_SERVO, LOW);
 
     Serial.begin(115200);
     initIBus(PIN_IBUS_RX);
     initAudio(); 
 
     delay(200);
     Serial.println("\n================================================");
     Serial.println("SYSTEM READY [v1.5]: BATTERY MONITOR & FAILSAFE ACTIVE.");
     Serial.println("================================================\n");
 }
 
 void loop() {
     bool hasValidPacket = readIBus();
     if (hasValidPacket) {
         lastPacketTime = millis();
     }
 
     int throttle  = channels[1]; 
     int steering  = channels[0]; 
     int turretRaw = channels[3]; 
     int ch3       = channels[2]; 
     int vrA       = channels[4]; 
     int vrB       = channels[5]; 
     int swa       = channels[6]; 
 
     bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
     bool ch3Failsafe = (ch3 <= 1000); 
     bool signalLost  = rxTimeout || ch3Failsafe; 
     bool isSwaArmed  = (swa > 1500);             
     bool isCentered  = isSticksCentered(throttle, steering, turretRaw);
 
     if (!signalLost) {
         lastValidVrA = vrA;
         lastValidVrB = vrB;
     }
 
     if (lastValidVrA < 1050) setAudioVolume(0.0f);
     else setAudioVolume((lastValidVrA - 1050) / 950.0f);
 
     updateMotorInertia(lastValidVrB);
     
     // --- Опрос системы питания ---
     updateBatteryMonitor();
     bool batteryDead = isBatteryLow();
 
     if (isFirstLoop) {
         if (batteryDead) currentState = STATE_LOW_BATTERY;
         else if (signalLost) currentState = STATE_DISCONNECTED;
         else if (!isSwaArmed) currentState = STATE_DISARMED;
         else currentState = STATE_STARTFAIL;
         previousState = currentState;
         warningStartTime = millis();
         isFirstLoop = false;
     }
 
     // --- ПРИОРИТЕТНОЕ ПРЕРЫВАНИЕ: РАЗРЯД АКБ ---
     if (batteryDead && currentState != STATE_LOW_BATTERY) {
         currentState = STATE_LOW_BATTERY;
     }
 
     if (currentState != previousState) {
         Serial.print("\n>>> STATE TRANSITION: ");
         Serial.print(stateNames[previousState]);
         Serial.print(" -> ");
         Serial.println(stateNames[currentState]);
         previousState = currentState;
     }
 
     if (millis() - lastDiagLogTime >= 1000) {
         lastDiagLogTime = millis();
         float inertiaSec = map(constrain(lastValidVrB, 1000, 2000), 1000, 2000, 0, MAX_INERTIA_TIME_MS) / 1000.0f;
         
         // Красивый вывод статуса сенсора
         char batStr[16];
         if (isBatterySensorConnected()) {
             snprintf(batStr, sizeof(batStr), "%.2fV", getBatteryVoltage());
         } else {
             snprintf(batStr, sizeof(batStr), "NO_SENS");
         }
 
         Serial.printf("[DIAG] State:%s | VBat:%s | signalLost:%d | SWA:%d | CH2:%d | VRB(sec):%.1fs\n",
                       stateNames[currentState], batStr, signalLost, swa, throttle, inertiaSec);
     }
 
     switch (currentState) {
       case STATE_LOW_BATTERY:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         setAudioMode(AUDIO_MODE_SIREN);
         break;
 
       case STATE_DISCONNECTED:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         if (millis() - warningStartTime < SIREN_MAX_DURATION_MS) setAudioMode(AUDIO_MODE_SIREN);
         else setAudioMode(AUDIO_MODE_MUTE);
         
         if (!signalLost) {
             if (!isSwaArmed) currentState = STATE_DISARMED;
             else currentState = STATE_STARTFAIL;
         }
         break;
 
       case STATE_DISARMED:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         setAudioMode(AUDIO_MODE_MUTE);
 
         if (signalLost) {
             currentState = STATE_DISCONNECTED;
             warningStartTime = millis();
         } else if (isSwaArmed) {
             if (isCentered) {
                 currentState = STATE_START;
                 startTimer = millis();
                 setAudioMode(AUDIO_MODE_START);
             } else {
                 currentState = STATE_STARTFAIL;
             }
         }
         break;
 
       case STATE_STARTFAIL:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         setAudioMode(AUDIO_MODE_SIREN);
 
         if (signalLost) {
             currentState = STATE_DISCONNECTED;
             warningStartTime = millis();
         } else if (!isSwaArmed) {
             currentState = STATE_DISARMED;
         } else if (isCentered) {
             currentState = STATE_START;
             startTimer = millis();
             setAudioMode(AUDIO_MODE_START);
         }
         break;
 
       case STATE_START:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         
         if (signalLost || !isSwaArmed) {
             currentState = STATE_SHUTDOWN;
             shutdownStartTime = millis();
             setAudioMode(AUDIO_MODE_STOP);
         } else if (millis() - startTimer >= START_DURATION_MS) {
             currentState = STATE_RUNNING;
             lastActivityTime = millis();
             setAudioMode(AUDIO_MODE_ENGINE); 
         }
         break;
 
       case STATE_RUNNING:
         if (signalLost || !isSwaArmed || (millis() - lastActivityTime > AUTO_SHUTDOWN_MS)) {
             currentState = STATE_SHUTDOWN;
             shutdownStartTime = millis();
             setAudioMode(AUDIO_MODE_STOP);
             break;
         }
 
         updateMixer(throttle, steering);
         
         if (!isCentered) lastActivityTime = millis(); 
         
         if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) turretRaw = CENTER_VAL;
         setTurretSpeed(map(turretRaw, 1000, 2000, -255, 255));
         updateEngineSound(throttle);
         break;
 
       case STATE_SHUTDOWN:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         
         if (millis() - shutdownStartTime >= 5000) {
             if (signalLost) {
                 currentState = STATE_DISCONNECTED;
                 warningStartTime = millis();
             } else if (!isSwaArmed) {
                 currentState = STATE_DISARMED;
             }
         }
         break;
     }
 }