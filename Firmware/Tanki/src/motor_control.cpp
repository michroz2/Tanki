/**
 * @file motor_control.cpp
 * @version 0.2.1
 * @brief Диагностическая реализация модуля ходовой части с прямой цифровой записью (без ШИМ).
 * Используется для проверки аппаратной части и исключения конфликтов версий Arduino Core ESP32.
 */

 #include "motor_control.h"

 // --- ФИЗИЧЕСКИЕ ПИНЫ УПРАВЛЕНИЯ ---
 const int PIN_L_PWM_FWD = 16;
 const int PIN_L_PWM_REV = 17;
 const int PIN_R_PWM_FWD = 19;
 const int PIN_R_PWM_REV = 23;
 
 void initMotors() {
     // Инициализируем пины как стандартные цифровые выходы общего назначения (GPIO)
     pinMode(PIN_L_PWM_FWD, OUTPUT);
     pinMode(PIN_L_PWM_REV, OUTPUT);
     pinMode(PIN_R_PWM_FWD, OUTPUT);
     pinMode(PIN_R_PWM_REV, OUTPUT);
 
     // Устанавливаем стартовое безопасное состояние (нули на всех линиях)
     setMotorSpeeds(0, 0);
 }
 
 void setMotorSpeeds(int leftSpeed, int rightSpeed) {
     // Логика переключения левой гусеницы (прямой постоянный сигнал HIGH / LOW)
     if (leftSpeed > 0) {
         digitalWrite(PIN_L_PWM_FWD, HIGH);
         digitalWrite(PIN_L_PWM_REV, LOW);
     } else if (leftSpeed < 0) {
         digitalWrite(PIN_L_PWM_FWD, LOW);
         digitalWrite(PIN_L_PWM_REV, HIGH);
     } else {
         digitalWrite(PIN_L_PWM_FWD, LOW);
         digitalWrite(PIN_L_PWM_REV, LOW);
     }
 
     // Логика переключения правой гусеницы (прямой постоянный сигнал HIGH / LOW)
     if (rightSpeed > 0) {
         digitalWrite(PIN_R_PWM_FWD, HIGH);
         digitalWrite(PIN_R_PWM_REV, LOW);
     } else if (rightSpeed < 0) {
         digitalWrite(PIN_R_PWM_FWD, LOW);
         digitalWrite(PIN_R_PWM_REV, HIGH);
     } else {
         digitalWrite(PIN_R_PWM_FWD, LOW);
         digitalWrite(PIN_R_PWM_REV, LOW);
     }
 }