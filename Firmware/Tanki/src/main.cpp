/**
 * @file main.cpp
 * @version 1.3
 * @brief Главный файл прошивки радиоуправляемого танка 1:16 (ESP32).
 * * v1.1: Миграция настроек в глобальный config.h
 * * v1.2: Расширена телеметрия [DIAG] (добавлены CH1-CH4) для аппаратной отладки.
 * * v1.3: Интегрирован вызов подсистемы инерции updateMotorInertia(), упрощен FSM SHUTDOWN.
 */

 #include <Arduino.h>
 #include "config.h"
 #include "ibus_parser.h"
 #include "motor_control.h"
 #include "mixer.h"
 #include "turret_control.h"
 #include "audio_system.h" 
 
 // Зона нечувствительности стиков (Deadband) для предотвращения дребезга в центре
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 /**
  * @brief Возможные состояния системы (Finite State Machine).
  */
 enum SystemState {
     STATE_DISCONNECTED, // Потеряна радиосвязь с пультом
     STATE_DISARMED,     // Связь есть, тумблер предохранителя (SWA) выключен (безопасно)
     STATE_STARTFAIL,    // Ошибка запуска (стик не в центре или SWA был включен при старте)
     STATE_START,        // Процесс запуска двигателя (играет start.wav, управление заблокировано)
     STATE_RUNNING,      // Штатный боевой режим (управление движением и башней)
     STATE_SHUTDOWN      // Процесс остановки/глушения с плавным торможением и блокировкой
 };
 
 // Инициализация стартовых переменных машины состояний
 SystemState currentState = STATE_DISCONNECTED;
 SystemState previousState = STATE_DISCONNECTED; 
 bool isFirstLoop = true;
 
 // Текстовые названия состояний для удобства логирования
 const char* stateNames[] = {"DISCONNECTED", "DISARMED", "STARTFAIL", "START", "RUNNING", "SHUTDOWN"};
 
 // Таймеры и таймауты
 unsigned long lastPacketTime = 0;
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3500; // Длительность фазы запуска двигателя
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; // Длительность сирены при потере связи
 
 unsigned long lastActivityTime = 0;
 const unsigned long AUTO_SHUTDOWN_MS = 30000; // Таймер простоя до автоотключения (30 сек)
 
 // Переменные для глушения и фикса громкости
 unsigned long shutdownStartTime = 0;
 int lastValidVrA = 2000; // Сохранение громкости (по умолчанию максимум)
 int lastValidVrB = 1000; // Кэш канала VRB (инерция) при потере связи (по умолчанию 0 сек)
 
 unsigned long lastDiagLogTime = 0; // Таймер для секундной телеметрии
 
 /**
  * @brief Проверка, находятся ли все основные стики в центральном положении.
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
     Serial.println("SYSTEM READY [v1.3]: INERTIA ENGINE ACTIVE.");
     Serial.println("================================================\n");
 }
 
 void loop() {
     // Чтение шины i-BUS. Если получен пакет, обновляем время последней связи.
     bool hasValidPacket = readIBus();
     if (hasValidPacket) {
         lastPacketTime = millis();
     }
 
     // Извлечение значений управляющих каналов из глобального массива
     int throttle  = channels[1]; // Канал 2: Газ (танковый микшер)
     int steering  = channels[0]; // Канал 1: Руль / Поворот
     int turretRaw = channels[3]; // Канал 4: Управление башней
     int ch3       = channels[2]; // Канал 3: Газ/Связь (Failsafe)
     int vrA       = channels[4]; // Канал 5: Крутилка громкости
     int vrB       = channels[5]; // Канал 6: Крутилка инерции (VRB)
     int swa       = channels[6]; // Канал 7: Тумблер предохранителя SWA
 
     // Определение системных флагов безопасности
     bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
     bool ch3Failsafe = (ch3 <= 1000); 
     bool signalLost  = rxTimeout || ch3Failsafe; // Потеря связи (таймаут или аппаратный failsafe)
     bool isSwaArmed  = (swa > 1500);             // Тумблер SWA включен (боевой режим)
     bool isCentered  = isSticksCentered(throttle, steering, turretRaw);
 
     // --- КЭШИРОВАНИЕ НАСТРОЕК (VR A / VR B) ПРИ СТАБИЛЬНОЙ СВЯЗИ ---
     if (!signalLost) {
         lastValidVrA = vrA;
         lastValidVrB = vrB;
     }
 
     // Применение громкости
     if (lastValidVrA < 1050) setAudioVolume(0.0f);
     else setAudioVolume((lastValidVrA - 1050) / 950.0f);
 
     // ВЫЗОВ НЕБЛОКИРУЮЩЕЙ ПОДСИСТЕМЫ ИНЕРЦИИ НА КАЖДОМ ИТЕРАЦИОННОМ ЦИКЛЕ
     updateMotorInertia(lastValidVrB);
     // ----------------------------------------------------------------
 
     // Первичная инициализация состояния при подаче питания или ресете МК
     if (isFirstLoop) {
         if (signalLost) currentState = STATE_DISCONNECTED;
         else if (!isSwaArmed) currentState = STATE_DISARMED;
         else currentState = STATE_STARTFAIL;
         previousState = currentState;
         warningStartTime = millis();
         isFirstLoop = false;
     }
 
     // Логирование моментов смены состояния системы
     if (currentState != previousState) {
         Serial.print("\n>>> STATE TRANSITION: ");
         Serial.print(stateNames[previousState]);
         Serial.print(" -> ");
         Serial.println(stateNames[currentState]);
         previousState = currentState;
     }
 
     // Периодическая секундная телеметрия для отладки
     if (millis() - lastDiagLogTime >= 1000) {
         lastDiagLogTime = millis();
         float inertiaSec = map(constrain(lastValidVrB, 1000, 2000), 1000, 2000, 0, MAX_INERTIA_TIME_MS) / 1000.0f;
         Serial.printf("[DIAG] State:%s | signalLost:%d | SWA:%d | CH1:%d | CH2:%d | CH3:%d | CH4:%d | CH6(VRB):%d | InertiaSec:%.2fs\n",
                       stateNames[currentState], signalLost, swa,
                       steering, throttle, ch3, turretRaw, lastValidVrB, inertiaSec);
     }
 
     // --- ГЛАВНАЯ МАШИНА СОСТОЯНИЙ (FSM) ---
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
             Serial.println("[DEBUG] SHUTDOWN TRIGGERED! Initiating smooth inertia brake...");
             break;
         }
 
         updateMixer(throttle, steering);
         
         if (!isCentered) lastActivityTime = millis(); 
         
         if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) turretRaw = CENTER_VAL;
         setTurretSpeed(map(turretRaw, 1000, 2000, -255, 255));
         updateEngineSound(throttle);
         break;
 
       case STATE_SHUTDOWN:
         // Передаем командование на гашение моторов подсистеме инерции
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         
         // Разрешаем переход дальше только после полного завершения файла stop.wav или таймаута (5 сек)
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