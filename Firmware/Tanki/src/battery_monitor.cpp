/**
 * @file battery_monitor.cpp
 * @version 1.5
 * @brief Реализация мониторинга батареи.
 */

 #include "battery_monitor.h"
 #include "config.h"
 
 static float filteredVoltage = 7.0f; 
 static unsigned long lowVoltageStartTime = 0;
 static bool lowBatteryFlag = false;
 
 void initBatteryMonitor() {
     pinMode(PIN_BAT_ADC, INPUT);
     // Делаем первичный "грубый" замер для заполнения фильтра
     float rawPinV = (analogRead(PIN_BAT_ADC) / 4095.0f) * 3.3f;
     filteredVoltage = rawPinV * ((BAT_R1_OHMS + BAT_R2_OHMS) / BAT_R2_OHMS);
 }
 
 void updateBatteryMonitor() {
     float rawPinV = (analogRead(PIN_BAT_ADC) / 4095.0f) * 3.3f; 
     float currentVoltage = rawPinV * ((BAT_R1_OHMS + BAT_R2_OHMS) / BAT_R2_OHMS);
 
     // EMA-фильтр (Сглаживает скачки)
     filteredVoltage = (filteredVoltage * 0.95f) + (currentVoltage * 0.05f);
 
     // ЗАЩИТА ОТ ДУРАКА: Сенсор оторван или не подключен (напряжение < 3.0В)
     if (filteredVoltage < BAT_DISCONNECTED_VOLTS) {
         lowVoltageStartTime = 0;
         lowBatteryFlag = false; // Разрешаем танку ехать без защиты
     }
     // РЕАЛЬНЫЙ РАЗРЯД: Напряжение в опасной зоне, но танк еще физически работает
     else if (filteredVoltage < BAT_CUTOFF_VOLTS) {
         if (lowVoltageStartTime == 0) {
             lowVoltageStartTime = millis(); // Запускаем таймер просадки
         } else if (millis() - lowVoltageStartTime > BAT_SAG_TIMEOUT_MS) {
             lowBatteryFlag = true; // Отключаем танк
         }
     } 
     // ВСЕ В НОРМЕ
     else {
         lowVoltageStartTime = 0; 
     }
 }
 
 float getBatteryVoltage() {
     return filteredVoltage;
 }
 
 bool isBatteryLow() {
     return lowBatteryFlag;
 }
 
 bool isBatterySensorConnected() {
     return (filteredVoltage >= BAT_DISCONNECTED_VOLTS);
 }