/**
 * @file main.cpp
 * @version 1.1
 * @brief Главный файл прошивки радиоуправляемого танка 1:16 (ESP32).
 * * Содержит реализацию конечного автомата (FSM), 
 * алгоритм безопасного инерционного торможения при потере связи, 
 * таймер автоотключения простоя (30 сек) и систему телеметрии.
 * * v1.1: Миграция настроек в глобальный config.h
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
 // SIGNAL_TIMEOUT_MS теперь берется из config.h
 
 unsigned long startTimer = 0;
 const unsigned long START_DURATION_MS = 3500; // Длительность фазы запуска двигателя
 
 unsigned long warningStartTime = 0; 
 const unsigned long SIREN_MAX_DURATION_MS = 15000; // Длительность сирены при потере связи
 
 unsigned long lastActivityTime = 0;
 const unsigned long AUTO_SHUTDOWN_MS = 30000; // Таймер простоя до автоотключения (30 сек)
 
 // Переменные для инерционного торможения и фикса громкости
 unsigned long shutdownStartTime = 0;
 int brakingStartThrottle = 1500;
 int brakingStartSteering = 1500;
 int lastThrottle = 1500;
 int lastSteering = 1500;
 int lastValidVrA = 2000; // Сохранение громкости (по умолчанию максимум)
 
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
     Serial.println("SYSTEM READY [v1.1]: HAL (config.h) INTEGRATED.");
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
     int swa       = channels[6]; // Канал 7: Тумблер предохранителя SWA
 
     // Определение системных флагов безопасности
     bool rxTimeout   = (millis() - lastPacketTime > SIGNAL_TIMEOUT_MS);
     bool ch3Failsafe = (ch3 <= 1000); 
     bool signalLost  = rxTimeout || ch3Failsafe; // Потеря связи (таймаут или аппаратный failsafe)
     bool isSwaArmed  = (swa > 1500);             // Тумблер SWA включен (боевой режим)
     bool isCentered  = isSticksCentered(throttle, steering, turretRaw);
 
     // --- ФИКС ГРОМКОСТИ ---
     // Кэшируем громкость только если связь стабильна.
     if (!signalLost) {
         lastValidVrA = vrA;
     }
 
     // Применяем закэшированную (или актуальную) громкость
     if (lastValidVrA < 1050) setAudioVolume(0.0f);
     else setAudioVolume((lastValidVrA - 1050) / 950.0f);
     // -------------------------------
 
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
         Serial.printf("[DIAG] State:%s | signalLost:%d | SWA:%d | Centered:%d\n",
                       stateNames[currentState], signalLost, swa, isCentered);
     }
 
     // --- ГЛАВНАЯ МАШИНА СОСТОЯНИЙ (FSM) ---
     switch (currentState) {
 
       case STATE_DISCONNECTED:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         // Сирена работает первые 15 секунд после обрыва связи, затем тишина
         if (millis() - warningStartTime < SIREN_MAX_DURATION_MS) setAudioMode(AUDIO_MODE_SIREN);
         else setAudioMode(AUDIO_MODE_MUTE);
         
         // Переход при восстановлении связи
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
             // Если тумблер включили: проверяем стики. Если в центре — запуск, иначе ошибка.
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
         setAudioMode(AUDIO_MODE_SIREN); // Ошибка оператора сопровождается сиреной
 
         if (signalLost) {
             currentState = STATE_DISCONNECTED;
             warningStartTime = millis();
         } else if (!isSwaArmed) {
             currentState = STATE_DISARMED;
         } else if (isCentered) {
             // Когда оператор вернул стики в центр — разрешаем запуск
             currentState = STATE_START;
             startTimer = millis();
             setAudioMode(AUDIO_MODE_START);
         }
         break;
 
       case STATE_START:
         setMotorSpeeds(0, 0);
         setTurretSpeed(0);
         
         // Экстренный уход в shutdown при обрыве связи или выключении SWA во время запуска
         if (signalLost || !isSwaArmed) {
             currentState = STATE_SHUTDOWN;
             shutdownStartTime = millis();
             brakingStartThrottle = 1500;
             brakingStartSteering = 1500;
             setAudioMode(AUDIO_MODE_STOP);
         } else if (millis() - startTimer >= START_DURATION_MS) {
             // По истечении времени стартового файла переходим в боевой режим
             currentState = STATE_RUNNING;
             lastActivityTime = millis();
             setAudioMode(AUDIO_MODE_ENGINE); 
         }
         break;
 
       case STATE_RUNNING:
         // КРИТИЧЕСКИ ВАЖНО: Проверка потери связи ДО применения стиков (защита от мусора при Failsafe OFF)
         if (signalLost || !isSwaArmed || (millis() - lastActivityTime > AUTO_SHUTDOWN_MS)) {
             currentState = STATE_SHUTDOWN;
             shutdownStartTime = millis();
             
             // Фиксируем последние ВАЛИДНЫЕ значения стиков перед разрывом связи для плавного тормоза
             brakingStartThrottle = lastThrottle;
             brakingStartSteering = lastSteering;
             
             setAudioMode(AUDIO_MODE_STOP);
             Serial.println("[DEBUG] SHUTDOWN TRIGGERED! Initiating safe 2-second brake...");
             break; // Немедленно выходим, исключая обработку нулевых данных приемника
         }
 
         // Сохраняем валидные данные стиков текущего цикла
         lastThrottle = throttle;
         lastSteering = steering;
         updateMixer(throttle, steering);
         
         // Сброс таймера автоотключения при любом движении стиков
         if (!isCentered) lastActivityTime = millis(); 
         
         // Управление башней
         if (turretRaw > DEADBAND_MIN && turretRaw < DEADBAND_MAX) turretRaw = CENTER_VAL;
         setTurretSpeed(map(turretRaw, 1000, 2000, -255, 255));
         updateEngineSound(throttle);
         break;
 
       case STATE_SHUTDOWN:
         unsigned long shutdownElapsed = millis() - shutdownStartTime;
         
         // ФАЗА 1: Плавное инерционное торможение в течение первых 2 секунд (2000 мс)
         if (shutdownElapsed < 2000) {
             float progress = (float)shutdownElapsed / 2000.0f;
             // Математическое сведение виртуальных стиков из текущей позиции обратно в центр (1500)
             int currT = brakingStartThrottle + (1500 - brakingStartThrottle) * progress;
             int currS = brakingStartSteering + (1500 - brakingStartSteering) * progress;
             updateMixer(currT, currS);
             setTurretSpeed(0);
         } 
         // ФАЗА 2: Жесткое удержание моторов в нуле в течение последующих 3 секунд (до 5000 мс)
         else if (shutdownElapsed < 5000) {
             setMotorSpeeds(0, 0);
             setTurretSpeed(0);
         } 
         // ФАЗА 3: Снятие блокировки и разрешенные переходы
         else {
             setMotorSpeeds(0, 0);
             setTurretSpeed(0);
             
             if (signalLost) {
                 currentState = STATE_DISCONNECTED;
                 warningStartTime = millis();
             } else if (!isSwaArmed) {
                 currentState = STATE_DISARMED;
             }
             // Если связь есть, но тумблер SWA всё еще физически поднят — висим в глухой блокировке SHUTDOWN.
         }
         break;
     }
 }