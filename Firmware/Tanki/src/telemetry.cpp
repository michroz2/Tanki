/**
 * @file telemetry.cpp
 * @version 1.6.11
 * @brief Имплементация библиотеки IBusBM для передачи телеметрии.
 * * v1.6.11: Переход на аппаратно разделенные пины (RX:18, TX:22) для исключения коллизий.
 */

 #include "telemetry.h"
 #include "config.h"
 #include "battery_monitor.h"
 #include <IBusBM.h>
 
 // Экземпляр библиотеки
 IBusBM IBusSensor;
 
 void initTelemetry() {
     // Инициализация UART1 через IBusBM на разделенных пинах.
     // IBUSBM_NOTIMER означает, что библиотека не использует внутренние таймерные прерывания ESP32,
     // а полагается на наш вызов loop() в главном цикле, что безопасно для FSM танка.
     IBusSensor.begin(Serial1, IBUSBM_NOTIMER, PIN_TELEMETRY_RX, PIN_TELEMETRY_TX);
 
     // Добавляем виртуальный датчик напряжения (Sensor Type 0x03).
     // Библиотека автоматически назначит ему адрес ID = 1.
     IBusSensor.addSensor(IBUSS_EXTV);
     
     Serial.println("[TELEMETRY] Библиотека IBusBM инициализирована (RX:18, TX:22 + Diode).");
 }
 
 void updateTelemetry() {
     // 1. Обязательный вызов для парсинга входящих PING от приемника
     IBusSensor.loop();
     
     // 2. Получение актуального напряжения
     float vBat = getBatteryVoltage();
     
     // Формат пульта требует напряжение в сотых долях (умножаем на 100)
     uint16_t outVal = isBatterySensorConnected() ? (uint16_t)(vBat * 100.0f) : 0;
     
     // 3. Передача значения в библиотеку.
     // Когда приемник запросит VALUE, библиотека аппаратно ответит этим значением.
     IBusSensor.setSensorMeasurement(1, outVal);
 }