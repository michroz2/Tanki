/**
 * @file telemetry.h
 * @version 1.6
 * @brief Модуль полудуплексной телеметрии i-BUS.
 */

 #pragma once
 #include <Arduino.h>
 
 /**
  * @brief Инициализация Serial1 для телеметрии в режиме Half-Duplex.
  */
 void initTelemetry();
 
 /**
  * @brief Неблокирующий парсинг запросов от приемника и отправка данных.
  * Должен вызываться в основном цикле loop().
  */
 void updateTelemetry();