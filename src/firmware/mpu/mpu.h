#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

// Inicializa o sensor MPU-6500 e configura suas escalas
void configurarMPU();

// Atualiza a integração do Yaw (deve ser chamada frequentemente no loop)
void mpu_update();

// Retorna o ângulo Yaw atual em graus
float mpu_get_yaw();

// Reseta o ângulo Yaw para 0
void mpu_reset_yaw();

#endif

/*

Quando For implementar a main.cpp

void setup() {
  Serial.begin(115200);
  // Outras configurações...
  Serial.println("[Micromouse] Configurando MPU-6500...");
  configurarMPU();
}

void loop() {
  // Outras lógicas...
  lerExibirMPU();
  // Outras lógicas...
  //divisor
  Serial.println("--------------------------------------------------");
}

*/