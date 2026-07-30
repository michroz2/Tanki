/**
 * @file turret_control.cpp
 * @version 1.1
 * @brief Реализация управления приводом башни с использованием подсистемы LEDC.
 * * v1.1: Миграция пинов и частоты в глобальный config.h
 */

 #include "turret_control.h"
 #include "config.h"
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 // Выделяем каналы 4 и 5 (каналы 0–3 заняты ходовой частью)
 const int PWM_CH_TURRET_IN1 = 4;
 const int PWM_CH_TURRET_IN2 = 5;
 
 void initTurret() {
     // 1. Настройка частоты и разрешения для выделенных каналов ШИМ (из config.h)
     ledcSetup(PWM_CH_TURRET_IN1, PWM_FREQUENCY, PWM_RESOLUTION);
     ledcSetup(PWM_CH_TURRET_IN2, PWM_FREQUENCY, PWM_RESOLUTION);
 
     // 2. Привязка физических пинов к каналам генератора (из config.h)
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