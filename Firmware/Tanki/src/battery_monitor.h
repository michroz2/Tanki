/**
 * @file battery_monitor.h
 * @version 1.5
 * @brief Модуль чтения АЦП и защиты аккумулятора от глубокого разряда.
 */

 #pragma once
 #include <Arduino.h>
 
 /**
  * @brief Настройка пина АЦП и первичный замер.
  */
 void initBatteryMonitor();
 
 /**
  * @brief Неблокирующее чтение АЦП с применением EMA-фильтра.
  * Должно вызываться в основном цикле.
  */
 void updateBatteryMonitor();
 
 /**
  * @brief Получить текущее отфильтрованное напряжение АКБ.
  * @return Напряжение в вольтах.
  */
 float getBatteryVoltage();
 
 /**
  * @brief Проверить, сработала ли отсечка по низкому напряжению.
  * @return true, если АКБ разряжен (требуется блокировка танка).
  */
 bool isBatteryLow();
 
 /**
  * @brief Проверить, физически ли подключен датчик напряжения.
  * @return true, если напряжение правдоподобно (>3.0В).
  */
 bool isBatterySensorConnected();