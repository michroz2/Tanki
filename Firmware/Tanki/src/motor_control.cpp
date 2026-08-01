/**
 * @file motor_control.cpp
 * @version 1.3
 * @brief Реализация управления ходовой частью с поддержкой неблокирующей инерции.
 * * v1.1: Миграция пинов и частоты в глобальный config.h
 * * v1.2: Реализован алгоритм плавной интерполяции скоростей (Slew Rate) по VRB (CH6).
 */

 #include "motor_control.h"
 #include "config.h"
 
 // --- НАСТРОЙКА АППАРАТНОГО ШИМ (LEDC) ---
 const int PWM_CH_L_FWD = 0;
 const int PWM_CH_L_REV = 1;
 const int PWM_CH_R_FWD = 2;
 const int PWM_CH_R_REV = 3;
 
 // --- ПЕРЕМЕННЫЕ ИНЕРЦИОННОЙ ПОДСИСТЕМЫ ---
 static float currentLeftSpeed = 0.0f;  // Текущая фактическая скорость левого мотора (-255.0f .. 255.0f)
 static float currentRightSpeed = 0.0f; // Текущая фактическая скорость правого мотора (-255.0f .. 255.0f)
 
 static int targetLeftSpeed = 0;        // Целевая скорость левого мотора (-255 .. 255)
 static int targetRightSpeed = 0;       // Целевая скорость правого мотора (-255 .. 255)
 
 static unsigned long lastMotorUpdate = 0; // Время последнего шага расчёта инерции
 
 /**
  * @brief Внутренняя функция непосредственной записи ШИМ в физические пины LEDC.
  */
 static void applyPhysicalPWM(int leftSpeed, int rightSpeed) {
     leftSpeed = constrain(leftSpeed, -255, 255);
     rightSpeed = constrain(rightSpeed, -255, 255);
 
     // Левая гусеница
     if (leftSpeed > 0) {
         ledcWrite(PWM_CH_L_FWD, leftSpeed);
         ledcWrite(PWM_CH_L_REV, 0);
     } else if (leftSpeed < 0) {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, -leftSpeed);
     } else {
         ledcWrite(PWM_CH_L_FWD, 0);
         ledcWrite(PWM_CH_L_REV, 0);
     }
 
     // Правая гусеница
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
 
 void initMotors() {
     ledcSetup(PWM_CH_L_FWD, PWM_FREQUENCY, PWM_RESOLUTION);
     ledcSetup(PWM_CH_L_REV, PWM_FREQUENCY, PWM_RESOLUTION);
     ledcSetup(PWM_CH_R_FWD, PWM_FREQUENCY, PWM_RESOLUTION);
     ledcSetup(PWM_CH_R_REV, PWM_FREQUENCY, PWM_RESOLUTION);
 
     ledcAttachPin(PIN_L_PWM_FWD, PWM_CH_L_FWD);
     ledcAttachPin(PIN_L_PWM_REV, PWM_CH_L_REV);
     ledcAttachPin(PIN_R_PWM_FWD, PWM_CH_R_FWD);
     ledcAttachPin(PIN_R_PWM_REV, PWM_CH_R_REV);
 
     currentLeftSpeed = 0.0f;
     currentRightSpeed = 0.0f;
     targetLeftSpeed = 0;
     targetRightSpeed = 0;
     lastMotorUpdate = millis();
 
     applyPhysicalPWM(0, 0);
 }
 
 void setMotorSpeeds(int leftSpeed, int rightSpeed) {
     // Задаем только целевые значения, ограниченные 8-битным диапазоном
     targetLeftSpeed = constrain(leftSpeed, -255, 255);
     targetRightSpeed = constrain(rightSpeed, -255, 255);
 }
 
 void updateMotorInertia(int vrbRaw) {
     unsigned long now = millis();
     unsigned long dtMs = now - lastMotorUpdate;
     
     // Предотвращаем частый перерасчет или деление на ноль, если с последнего прохода не прошло времени
     if (dtMs == 0) return;
     lastMotorUpdate = now;
 
     // Ограничиваем значение ручки VRB валидным диапазоном i-BUS
     vrbRaw = constrain(vrbRaw, 1000, 2000);
 
     // Вычисляем целевое время полного разгона (от 0 до 255 единиц ШИМ) в миллисекундах
     // 1000 мкс -> 0 мс (мгновенно), 2000 мкс -> MAX_INERTIA_TIME_MS (5000 мс)
     uint32_t rampTimeMs = map(vrbRaw, 1000, 2000, 0, MAX_INERTIA_TIME_MS);
 
     // Если инерция отключена (VRB близка к минимуму) — мгновенно применяем целевые скорости
     if (rampTimeMs < 50) {
         currentLeftSpeed = (float)targetLeftSpeed;
         currentRightSpeed = (float)targetRightSpeed;
         applyPhysicalPWM(targetLeftSpeed, targetRightSpeed);
         return;
     }
 
     // Скорость изменения ШИМ-единиц за 1 миллисекунду: (255 единиц / rampTimeMs)
     float maxStep = (255.0f / (float)rampTimeMs) * (float)dtMs;
 
     // Плавное приближение левого мотора к целевому значению
     if (currentLeftSpeed < targetLeftSpeed) {
         currentLeftSpeed += maxStep;
         if (currentLeftSpeed > targetLeftSpeed) currentLeftSpeed = (float)targetLeftSpeed;
     } else if (currentLeftSpeed > targetLeftSpeed) {
         currentLeftSpeed -= maxStep;
         if (currentLeftSpeed < targetLeftSpeed) currentLeftSpeed = (float)targetLeftSpeed;
     }
 
     // Плавное приближение правого мотора к целевому значению
     if (currentRightSpeed < targetRightSpeed) {
         currentRightSpeed += maxStep;
         if (currentRightSpeed > targetRightSpeed) currentRightSpeed = (float)targetRightSpeed;
     } else if (currentRightSpeed > targetRightSpeed) {
         currentRightSpeed -= maxStep;
         if (currentRightSpeed < targetRightSpeed) currentRightSpeed = (float)targetRightSpeed;
     }
 
     // Запись расчитанных физических значений в аппаратный ШИМ
     applyPhysicalPWM((int)currentLeftSpeed, (int)currentRightSpeed);
 }