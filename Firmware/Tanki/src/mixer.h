/**
 * @file mixer.h
 * @version 1.8
 * @brief Танковый микшер (Газ + Руль)
 * * v1.8: Добавлена поддержка тумблера SWB (CH8) для переключения на автомобильный реверс.
 */

 #pragma once

 /**
  * @brief Расчет скоростей гусениц на основе стика хода/поворота.
  * @param throttleRaw Значение канала газа (1000-2000)
  * @param steeringRaw Значение канала руля (1000-2000)
  * @param swbRaw Значение канала тумблера SWB (1000-2000)
  */
 void updateMixer(int throttleRaw, int steeringRaw, int swbRaw);