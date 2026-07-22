/**
 * @file motor_control.cpp
 * @version 0.2
 * @brief Реализация управления ходовой частью с использованием подсистемы LEDC.
 */

 #include "motor_control.h"

 // --- ПИНЫ УПРАВЛЕНИЯ ---
 const int PIN_L_PWM_FWD = 16;
 const int PIN_L_PWM_REV = 17;
 const int PIN_R_PWM_FWD = 19;
 const int PIN_R_PWM_REV = 23;
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 // ESP32 имеет 16 независимых каналов генерации ШИМ. Выделяем первые 4 для ходовой.
 const int PWM_CH_L_FWD = 0;
 const int PWM_CH_L_REV = 1;
 const int PWM_CH_R_FWD = 2;
 const int PWM_CH_R_REV = 3;
 
 // Частота 20 кГц гарантирует отсутствие "писка" в обмотках моторов
 const int PWM_FREQ = 20000; 
 // 8 бит дают нам шкалу мощности от 0 до 255
 const int PWM_RES = 8;      
 
 void initMotors() {
     // 1. Настройка частоты и разрешения для выделенных каналов
     ledcSetup(PWM_CH_L_FWD, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_L_REV, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_R_FWD, PWM_FREQ, PWM_RES);
     ledcSetup(PWM_CH_R_REV, PWM_FREQ, PWM_RES);
 
     // 2. Аппаратная привязка физических пинов к каналам генератора
     ledcAttachPin(PIN_L_PWM_FWD, PWM_CH_L_FWD);
     ledcAttachPin(PIN_L_PWM_REV, PWM_CH_L_REV);
     ledcAttachPin(PIN_R_PWM_FWD, PWM_CH_R_FWD);
     ledcAttachPin(PIN_R_PWM_REV, PWM_CH_R_REV);
 
     // 3. Блокировка: принудительная подача 0 на все каналы
     setMotorSpeeds(0, 0);
 }
 
 void setMotorSpeeds(int leftSpeed, int rightSpeed) {
     // Жесткое ограничение входных значений математическими рамками 8 бит
     leftSpeed = constrain(leftSpeed, -255, 255);
     rightSpeed = constrain(rightSpeed, -255, 255);
 
     // Логика переключения левой гусеницы
     if (leftSpeed > 0) {
         ledcWrite(PWM_CH_L_FWD, leftSpeed);
         ledcWrite(PWM_CH_L_REV, 0);
     } else if (leftSpeed < 0) {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, -leftSpeed); // Берем значение по модулю
     } else {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, 0);
     }
 
     // Логика переключения правой гусеницы
     if (rightSpeed > 0) {
         ledcWrite(PWM_CH_R_FWD, rightSpeed);
         ledcWrite(PWM_CH_R_REV, 0);
     } else if (rightSpeed < 0) {
         ledcWrite(PWM_CH_R_FWD, 0);
         ledcWrite(PWM_CH_R_REV, -rightSpeed);
     } else {
         ledcWrite(PWM_CH_R_FWD, 0);
         ledcWrite(PWM_CH_R_REV, 0);
     }
 }