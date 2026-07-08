#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

// Inicializa o sensor MPU-6500 e configura suas escalas
void configurarMPU();

// Lê e exibe a Aceleração (X, Y) e o Giroscópio (Z)
void lerExibirMPU();

// Mede o bias (offset) do giroscópio em Z parado, para descontar da integração.
// Chamar uma vez no setup(), logo após configurarMPU(), com o robô parado e nivelado.
void mpu_calibrar_offset_giro();

// Integra a velocidade angular do eixo Z (descontado o offset) desde a última
// chamada e acumula no ângulo total girado, em graus. Precisa ser chamada com
// frequência (a cada iteração de um loop de espera) para a integração ser precisa.
void mpu_atualizar_angulo();

// Zera o acumulador de ângulo. Chamar antes de iniciar um giro.
void mpu_zerar_angulo();

// Ângulo acumulado (graus) desde a última chamada a mpu_zerar_angulo().
float mpu_get_angulo();

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