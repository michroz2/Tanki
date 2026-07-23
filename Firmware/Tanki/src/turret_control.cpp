/**
 * @file turret_control.cpp
 * @version 0.4
 * @brief Реализация управления приводом башни с использованием подсистемы LEDC.
 */

 #include "turret_control.h"

 // --- ФИЗИЧЕСКИЕ ПИНЫ УПРАВЛЕНИЯ БАШНЕЙ ---
 const int PIN_TURRET_IN1 = 32;
 const int PIN_TURRET_IN2 = 33;
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 // Выделяем каналы 4 и 5 (каналы 0–3 заняты ходовой частью)
 const int PWM_CH_TURRET_IN1 = 4;
 const int PWM_CH_TURRET_IN2 = 5;
 
 // Частота 20 кГц (унифицирована с ходовой частью)
 const int PWM_FREQ = 20000; 
 // Разрешение 8 бит (шкала мощности 0..255)
 const int PWM_RES = 8;      
 
 void initTurret() {
     // 1. Настройка частоты и разрешения для выделенных каналов ШИМ
     ledcSetup(PWM_CH_TURRET_IN1, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_TURRET_IN2, PWM_FREQ, PWM_RES);
 
     // 2. Привязка физических пинов к каналам генератора
     ledcAttachPin(PIN_TURRET_IN1, PWM_CH_TURRET_IN1);
     ledcAttachPin(PIN_TURRET_IN2, PWM_CH_TURRET_IN2);
 
     // 3. Блокировка: начальное состояние — остановка
     setTurretSpeed(0);
 }
 
 void setTurretSpeed(int speed) {
     // Ограничение входного значения в пределах 8 бит
     speed = constrain(speed, -255, 255);
 
     if (speed > 0) {
         ledcWrite(PWM_CH_TURRET_IN1, speed);
         ledcWrite(PWM_CH_TURRET_IN2, 0);
     } else if (speed < 0) {
         ledcWrite(PWM_CH_TURRET_IN1, 0);
         ledcWrite(PWM_CH_TURRET_IN2, -speed);
     } else {
         ledcWrite(PWM_CH_TURRET_IN1, 0);
         ledcWrite(PWM_CH_TURRET_IN2, 0);
     }
 }