/**
 * @file mixer.cpp
 * @version 0.3
 * @brief Реализация танкового микшера.
 */

 #include "mixer.h"
 #include "motor_control.h"
 
 // Константы мертвой зоны стиков (чтобы танк не "полз", если стик чуть-чуть сбит)
 const int DEADBAND_MIN = 1470;
 const int DEADBAND_MAX = 1530;
 const int CENTER_VAL = 1500;
 
 void updateMixer(int throttle, int steering) {
     // 1. Применяем мертвую зону (отсекаем дрожание стиков в центре)
     if (throttle > DEADBAND_MIN && throttle < DEADBAND_MAX) throttle = CENTER_VAL;
     if (steering > DEADBAND_MIN && steering < DEADBAND_MAX) steering = CENTER_VAL;
 
     // 2. Преобразуем микросекунды (1000...2000) в диапазон ШИМ мощности (-255...+255)
     int y = map(throttle, 1000, 2000, -255, 255);
     int x = map(steering, 1000, 2000, -255, 255);
 
     // 3. Классическая математика дифференциального привода (Танковый микшер)
     int leftSpeed = y + x;
     int rightSpeed = y - x;
 
     // 4. Жестко ограничиваем итоговые значения пределами 8 бит, 
     // чтобы при крайних диагональных положениях стика не было переполнения
     leftSpeed = constrain(leftSpeed, -255, 255);
     rightSpeed = constrain(rightSpeed, -255, 255);
 
     // 5. Отправляем рассчитанные скорости в модуль управления моторами
     setMotorSpeeds(leftSpeed, rightSpeed);
 }