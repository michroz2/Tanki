/**
 * @file config.h
 * @version 1.7
 * @brief Hardware Abstraction Layer и глобальные настройки проекта Tanki
 * @details v1.7: Добавлен PIN_TELEMETRY_RX (GPIO18) для аппаратного разделения UART.
 */

 #ifndef CONFIG_H
 #define CONFIG_H
 
 // ==========================================
 // 1. АППАРАТНЫЕ НАСТРОЙКИ (PINOUT)
 // ==========================================
 #if defined(TANK_BOARD_LOLIN) || defined(TANK_BOARD_DEVKIT)
     
     // --- Радиоуправление (FlySky i-BUS) ---
     #define PIN_IBUS_RX          34  // Вход UART2 RX (Управление, только чтение)
     
     // --- Телеметрия (IBusBM UART1) ---
     #define PIN_TELEMETRY_TX     22  // TX с диодом
     #define PIN_TELEMETRY_RX     18  // RX прямой
 
     // --- Ходовая часть (BTS7960 - 4 канала ШИМ) ---
     #define PIN_L_PWM_FWD        16
     #define PIN_L_PWM_REV        17
     #define PIN_R_PWM_FWD        19
     #define PIN_R_PWM_REV        23
     #define PIN_SERVO            14  
 
     // --- Башня (DRV8833 - 2 канала ШИМ) ---
     #define PIN_TURRET_IN1       32
     #define PIN_TURRET_IN2       33
 
     // --- Аудио подсистема I2S (MAX98357A) ---
     #define PIN_I2S_BCLK         26
     #define PIN_I2S_LRC          25
     #define PIN_I2S_DIN          27
 
     // --- Мониторинг питания ---
     #define PIN_BAT_ADC          35  
 
 #else
     #error "Критическая ошибка компиляции: Плата не выбрана!"
 #endif
 
 // ==========================================
 // 2. НАСТРОЙКИ ЛОГИКИ И МОДЕЛИ ТАНКА
 // ==========================================
 #if defined(TANK_MODEL_T3)
     #define TANK_MAX_SPEED       255
     #define TANK_TURRET_SPEED    150
     #define AUDIO_START_FILE     "/start.wav"
     #define AUDIO_IDLE_FILE      "/idle.wav"
     #define AUDIO_STOP_FILE      "/stop.wav"
 
 #elif defined(TANK_MODEL_T4)
     #define TANK_MAX_SPEED       255
     #define TANK_TURRET_SPEED    150
     #define AUDIO_START_FILE     "/start.wav"
     #define AUDIO_IDLE_FILE      "/idle.wav"
     #define AUDIO_STOP_FILE      "/stop.wav"
 #else
     #error "Критическая ошибка компиляции: Модель танка не выбрана!"
 #endif
 
 // ==========================================
 // 3. ОБЩИЕ СИСТЕМНЫЕ НАСТРОЙКИ
 // ==========================================
 #define SIGNAL_TIMEOUT_MS    100    
 #define EMERGENCY_COUNTER    200    
 #define PWM_FREQUENCY        20000  
 #define PWM_RESOLUTION       8      
 #define MAX_INERTIA_TIME_MS  5000   
 
 #define BAT_R1_OHMS             9800.0f 
 #define BAT_R2_OHMS             4600.0f 
 #define BAT_CUTOFF_VOLTS        5.0f    
 #define BAT_DISCONNECTED_VOLTS  3.0f    
 #define BAT_SAG_TIMEOUT_MS      3000    
 
 // Настройки телеметрии i-BUS
 #define TELEMETRY_BAUD       115200
 
 #endif // CONFIG_H