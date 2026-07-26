/**
 * @file main.cpp
 * @version 0.11-LOG
 * @brief Главный файл прошивки (Переходный этап к новой FSM).
 * Цель: Проверить устранение Deadlock-а I2S при переключении тумблеров.
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
 
 enum SystemState {
     STATE_DISARMED, 
     STATE_WARNING,  
     STATE_STARTING, 
     STATE_RUNNING   
 };
 
 SystemState currentState = STATE_DISARMED;
 SystemState previousState = STATE_DISARMED; 
 const char* stateNames[] = {"DISARMED", "WARNING", "STARTING", "RUNNING"};
 
 unsigned long lastPacketTime = 0;
 const unsigned long SIGNAL_TIMEOUT_MS = 200; 
 
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3000; 
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; 
 unsigned long lastDiagLogTime = 0; 
 
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
   Serial.println("SYSTEM READY [v0.11-LOG]: I2S DEADLOCK FIX TEST.");
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
   int swa       = channels[6]; 
 
   if (vrA < 1050) {
       setAudioVolume(0.0f);
   } else {
       float vol = (vrA - 1050) / 950.0f;
       setAudioVolume(vol);
   }
 
   bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
   bool ch3Failsafe = (ch3 <= 1000); 
   bool signalLost  = rxTimeout || ch3Failsafe;
   bool isSwaArmed  = (swa > 1500);
 
   if (currentState != previousState) {
       Serial.print("\n>>> STATE TRANSITION: ");
       Serial.print(stateNames[previousState]);
       Serial.print(" -> ");
       Serial.println(stateNames[currentState]);
       previousState = currentState;
   }
 
   if (millis() - lastDiagLogTime >= 1000) {
       lastDiagLogTime = millis();
       Serial.printf("[DIAG] State:%s | signalLost:%d | isSwaArmed:%d | Centered:%d | CH3:%d | SWA:%d\n",
                     stateNames[currentState], signalLost, isSwaArmed, 
                     isSticksCentered(throttle, steering, turretRaw), ch3, swa);
   }
 
   if (signalLost) {
     if (currentState != STATE_WARNING) {
       currentState = STATE_WARNING;
       warningStartTime = millis(); 
     }
   } else if (!isSwaArmed) {
     currentState = STATE_DISARMED;
   }
 
   switch (currentState) {
     case STATE_DISARMED:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       setAudioMode(AUDIO_MODE_MUTE);
 
       if (isSwaArmed && !signalLost) {
         if (isSticksCentered(throttle, steering, turretRaw)) {
           currentState = STATE_STARTING;
           startTimer = millis();
           setAudioMode(AUDIO_MODE_START);
         } else {
           currentState = STATE_WARNING;
           warningStartTime = millis();
         }
       }
       break;
 
     case STATE_WARNING:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
 
       if (millis() - warningStartTime < SIREN_MAX_DURATION_MS) {
         setAudioMode(AUDIO_MODE_SIREN);
       } else {
         setAudioMode(AUDIO_MODE_MUTE); 
       }
 
       if (isSwaArmed && !signalLost) {
         Serial.println("[DEBUG] WARNING -> Signal restored & SWA armed. Checking sticks...");
         if (isSticksCentered(throttle, steering, turretRaw)) {
           Serial.println("[DEBUG] Sticks are centered! Triggering STARTING...");
           currentState = STATE_STARTING;
           startTimer = millis();
           setAudioMode(AUDIO_MODE_START);
           Serial.println("[DEBUG] setAudioMode(AUDIO_MODE_START) executed safely (No Deadlock).");
         } else {
           static unsigned long lastSticksLog = 0;
           if (millis() - lastSticksLog > 500) {
               lastSticksLog = millis();
               Serial.printf("[DEBUG] Waiting for sticks center. Throt:%d, Steer:%d, Turret:%d\n", throttle, steering, turretRaw);
           }
         }
       }
       break;
 
     case STATE_STARTING:
       setMotorSpeeds(0, 0);
       setTurretSpeed(0);
       
       if (millis() - startTimer >= START_DURATION_MS) {
         currentState = STATE_RUNNING;
         setAudioMode(AUDIO_MODE_ENGINE);
       }
       break;
 
     case STATE_RUNNING:
       updateMixer(throttle, steering);
       
       if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) {
         turretRaw = CENTER_VAL;
       }
       int turretSpeed = map(turretRaw, 1000, 2000, -255, 255);
       setTurretSpeed(turretSpeed);
       
       updateEngineSound(throttle);
       break;
   }
 }