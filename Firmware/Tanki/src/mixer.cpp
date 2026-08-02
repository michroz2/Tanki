/**
 * @file mixer.cpp
 * @version 1.8
 * @brief Танковый микшер
 * * v1.8: Внедрена логика автомобильного реверса (Car-like steering) по тумблеру SWB.
 */

 #include "mixer.h"
 #include "motor_control.h"
 #include <Arduino.h>
 
 // Мертвые зоны стиков (чтобы танк не гудел, когда стик в центре)
 const int MIXER_DEADBAND_MIN = 1470;
 const int MIXER_DEADBAND_MAX = 1530;
 
 void updateMixer(int throttleRaw, int steeringRaw, int swbRaw) {
     int thr = 0;
     int str = 0;
 
     // 1. Очистка от мертвых зон и маппинг в диапазон ШИМ (-255..255)
     if (throttleRaw < MIXER_DEADBAND_MIN) thr = map(throttleRaw, 1000, MIXER_DEADBAND_MIN, -255, 0);
     else if (throttleRaw > MIXER_DEADBAND_MAX) thr = map(throttleRaw, MIXER_DEADBAND_MAX, 2000, 0, 255);
 
     if (steeringRaw < MIXER_DEADBAND_MIN) str = map(steeringRaw, 1000, MIXER_DEADBAND_MIN, -255, 0);
     else if (steeringRaw > MIXER_DEADBAND_MAX) str = map(steeringRaw, MIXER_DEADBAND_MAX, 2000, 0, 255);
 
     // Защита от выбросов аппаратуры
     thr = constrain(thr, -255, 255);
     str = constrain(str, -255, 255);
 
     // =========================================================
     // 2. ЛОГИКА АВТОМОБИЛЬНОГО РЕВЕРСА (CAR-LIKE REVERSE)
     // Если тумблер SWB переключен вниз (>1500) И танк едет назад
     // =========================================================
     if (swbRaw > 1500 && thr < 0) {
         str = -str; // Инвертируем влияние руля на гусеницы
     }
 
     // 3. Классическое дифференциальное смешивание (V-Tail / Tank mix)
     int left = thr + str;
     int right = thr - str;
 
     // 4. Жесткое ограничение итоговой ШИМ перед отправкой в физику
     left = constrain(left, -255, 255);
     right = constrain(right, -255, 255);
 
     // 5. Передача ЦЕЛЕВЫХ скоростей в модуль моторов (там отработает инерция VRB)
     setMotorSpeeds(left, right);
 }