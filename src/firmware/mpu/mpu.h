#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

// Inicializa o sensor MPU-6500 e configura suas escalas
void configurarMPU();

// Lê e exibe a Aceleração (X, Y) e o Giroscópio (Z)
void lerExibirMPU();

// ── Integração do Giroscópio Z (Yaw) ─────────────────
// Deve ser chamada a cada ciclo do loop para integrar o giro Z
void atualizarMPU();

// Retorna o ângulo yaw acumulado em graus (positivo = anti-horário)
float obterYaw();

// Zera o acumulador de yaw (chamar ao alinhar o robô)
void resetarYaw();

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