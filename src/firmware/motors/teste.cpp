#include <Arduino.h>
#include "motors.h"

static constexpr int VEL_TESTE = 150;

void loop() {

    Serial.println("[TESTE 1/5] Motor ESQUERDO para FRENTE...");
    Serial.println("  Esperado: roda esquerda gira no sentido de avanco.");
    motor_esquerdo_set(VEL_TESTE);
    delay(3000);
    motor_esquerdo_set(0);
    delay(1500);

    Serial.println("[TESTE 2/5] Motor ESQUERDO para TRAS...");
    Serial.println("  Esperado: roda esquerda gira no sentido inverso.");
    motor_esquerdo_set(-VEL_TESTE);
    delay(3000);
    motor_esquerdo_set(0);
    delay(1500);

    Serial.println("[TESTE 3/5] Motor DIREITO para FRENTE...");
    Serial.println("  Esperado: roda direita gira no sentido de avanco.");
    motor_direito_set(VEL_TESTE);
    delay(3000);
    motor_direito_set(0);
    delay(1500);

    Serial.println("[TESTE 4/5] Motor DIREITO para TRAS...");
    Serial.println("  Esperado: roda direita gira no sentido inverso.");
    motor_direito_set(-VEL_TESTE);
    delay(3000);
    motor_direito_set(0);
    delay(1500);

    Serial.println("[TESTE 5/5] Rampa de VELOCIDADE (ambos os motores)...");
    Serial.println("  Esperado: aceleracao e desaceleracao suaves e simetricas.");

    for (int v = 0; v <= 255; v += 15) {
        motor_esquerdo_set(v);
        motor_direito_set(v);
        delay(150);
    }
    for (int v = 255; v >= 0; v -= 15) {
        motor_esquerdo_set(v);
        motor_direito_set(v);
        delay(150);
    }

    motors_coast_all(); 
    delay(300);
    motors_stop_all();  

    Serial.println("\n=======================================");
    Serial.println(" FIM DO CICLO. Reiniciando em 5 s...");
    Serial.println("=======================================\n");
    delay(5000);
}
