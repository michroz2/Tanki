/**
 * @file main.cpp
 * @version 0.21-LOG
 * @brief Главный файл прошивки. 
 * Фикс бага инерционного торможения при отключенном Failsafe приемника.
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
     STATE_DISCONNECTED, 
     STATE_DISARMED, 
     STATE_STARTFAIL,  
     STATE_START, 
     STATE_RUNNING,
     STATE_SHUTDOWN   
 };
 
 SystemState currentState = STATE_DISCONNECTED;
 SystemState previousState = STATE_DISCONNECTED; 
 bool isFirstLoop = true;
 
 const char* stateNames[] = {"DISCONNECTED", "DISARMED", "STARTFAIL", "START", "RUNNING", "SHUTDOWN"};
 
 unsigned long lastPacketTime = 0;
 const unsigned long SIGNAL_TIMEOUT_MS = 200; 
 
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3500; 
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; 
 
 unsigned long lastActivityTime = 0;
 const unsigned long AUTO_SHUTDOWN_MS = 30000; 
 
 unsigned long shutdownStartTime = 0;
 int brakingStartThrottle = 1500;
 int brakingStartSteering = 1500;
 int lastThrottle = 1500;
 int lastSteering = 1500;
 
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
   Serial.println("SYSTEM READY [v0.21-LOG]: FAILSAFE BRAKING FIX.");
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
 
   if (vrA < 1050) setAudioVolume(0.0f);
   else setAudioVolume((vrA - 1050) / 950.0f);
 
   bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
   bool ch3Failsafe = (ch3 <= 1000); 
   bool signalLost  = rxTimeout || ch3Failsafe;
   bool isSwaArmed  = (swa > 1500);
   bool isCentered  = isSticksCentered(throttle, steering, turretRaw);
 
   if (isFirstLoop) {
       if (signalLost) currentState = STATE_DISCONNECTED;
       else if (!isSwaArmed) currentState = STATE_DISARMED;
       else currentState = STATE_STARTFAIL;
       previousState = currentState;
       warningStartTime = millis();
       isFirstLoop = false;
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
       Serial.printf("[DIAG] State:%s | signalLost:%d | SWA:%d | Centered:%d\n",
                     stateNames[currentState], signalLost, swa, isCentered);
   }
 
   switch (currentState) {
 
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
           brakingStartThrottle = 1500;
           brakingStartSteering = 1500;
           setAudioMode(AUDIO_MODE_STOP);
       } else if (millis() - startTimer >= START_DURATION_MS) {
           currentState = STATE_RUNNING;
           lastActivityTime = millis();
           setAudioMode(AUDIO_MODE_ENGINE); 
       }
       break;
 
     case STATE_RUNNING:
       // ВАЖНО: 1. Проверяем аварии ДО применения стиков!
       if (signalLost || !isSwaArmed || (millis() - lastActivityTime > AUTO_SHUTDOWN_MS)) {
           currentState = STATE_SHUTDOWN;
           shutdownStartTime = millis();
           
           // Берем последние ВАЛИДНЫЕ значения стиков с прошлого цикла (а не текущие нули)
           brakingStartThrottle = lastThrottle;
           brakingStartSteering = lastSteering;
           
           setAudioMode(AUDIO_MODE_STOP);
           Serial.println("[DEBUG] SHUTDOWN TRIGGERED! Initiating safe 2-second brake...");
           break; // Немедленно выходим из case, чтобы не крутить моторы мусором
       }
 
       // 2. Если всё ОК — едем как обычно и сохраняем валидные стики
       lastThrottle = throttle;
       lastSteering = steering;
       updateMixer(throttle, steering);
       
       if (!isCentered) lastActivityTime = millis(); 
       
       if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) turretRaw = CENTER_VAL;
       setTurretSpeed(map(turretRaw, 1000, 2000, -255, 255));
       updateEngineSound(throttle);
       break;
 
     case STATE_SHUTDOWN:
       unsigned long shutdownElapsed = millis() - shutdownStartTime;
       
       if (shutdownElapsed < 2000) {
           float progress = (float)shutdownElapsed / 2000.0f;
           int currT = brakingStartThrottle + (1500 - brakingStartThrottle) * progress;
           int currS = brakingStartSteering + (1500 - brakingStartSteering) * progress;
           updateMixer(currT, currS);
           setTurretSpeed(0);
       } 
       else if (shutdownElapsed < 5000) {
           setMotorSpeeds(0, 0);
           setTurretSpeed(0);
       } 
       else {
           setMotorSpeeds(0, 0);
           setTurretSpeed(0);
           
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