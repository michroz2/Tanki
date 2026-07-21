#include <Arduino.h>

// --- ПИНЫ УПРАВЛЕНИЯ МОТОРАМИ ---
// Левая гусеница (драйвер BTS_L)
const int PIN_L_PWM_FWD = 16; 
const int PIN_L_PWM_REV = 17;

// Правая гусеница (драйвер BTS_R)
const int PIN_R_PWM_FWD = 19; 
const int PIN_R_PWM_REV = 23;

// Поворот башни (драйвер DRV8833)
const int PIN_TURRET_IN1 = 32;
const int PIN_TURRET_IN2 = 33;

void setup() {
  // 1. Настраиваем все пины моторов как выходы
  pinMode(PIN_L_PWM_FWD, OUTPUT);
  pinMode(PIN_L_PWM_REV, OUTPUT);
  pinMode(PIN_R_PWM_FWD, OUTPUT);
  pinMode(PIN_R_PWM_REV, OUTPUT);
  pinMode(PIN_TURRET_IN1, OUTPUT);
  pinMode(PIN_TURRET_IN2, OUTPUT);

  // 2. ПРИНУДИТЕЛЬНО ГЛУШИМ ВСЕ СИГНАЛЫ (выдаем логический 0)
  // Это предотвратит рывок танка при подаче силового питания
  digitalWrite(PIN_L_PWM_FWD, LOW);
  digitalWrite(PIN_L_PWM_REV, LOW);
  digitalWrite(PIN_R_PWM_FWD, LOW);
  digitalWrite(PIN_R_PWM_REV, LOW);
  digitalWrite(PIN_TURRET_IN1, LOW);
  digitalWrite(PIN_TURRET_IN2, LOW);

  // 3. Запускаем монитор порта для связи с компьютером
  Serial.begin(115200);
  
  // Даем процессору микро-паузу на инициализацию порта
  delay(100); 
  
  Serial.println("\n================================================");
  Serial.println("SYSTEM SAFE: All motor pins are forced LOW.");
  Serial.println("Safety Lock is active. You can connect main power.");
  Serial.println("================================================\n");
}

void loop() {
  // В бесконечном цикле пока ничего не происходит.
  // Плата просто держит нули на всех выходах.
} 