#include <Arduino.h>
#include "../encoder/encoder.h"
#include "../motors/motors.h"

unsigned long ultimo_tempo_print = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=========================================");
    Serial.println("   TESTE DE ENCODER E MOTORES INICIADO   ");
    Serial.println("=========================================\n");
    Serial.println("COMANDOS VIA SERIAL:");
    Serial.println(" [R] - Resetar contagem dos encoders");
    Serial.println(" [W] - Ligar ambos motores devagar (frente)");
    Serial.println(" [S] - Parar todos os motores");
    Serial.println(" [E] - Ligar SOMENTE motor esquerdo");
    Serial.println(" [D] - Ligar SOMENTE motor direito");
    Serial.println("-----------------------------------------\n");
    Serial.println("Para calibrar o PPR: deixe os motores em [S] (parados),");
    Serial.println("gire a roda exatamente 1 volta completa com a mão e");
    Serial.println("veja a contagem no terminal!\n");

    encoders_init();
    motors_init();
}

void loop() {
    // Leitura de Comandos pela Serial
    if (Serial.available()) {
        char c = Serial.read();
        c = toupper(c);

        if (c == 'R') {
            encoder_esquerdo_reset();
            encoder_direito_reset();
            Serial.println("=> Encoders resetados para 0!");
        } 
        else if (c == 'W') {
            motor_esquerdo_set(100);
            motor_direito_set(100);
            Serial.println("=> Motores Frente (Velocidade 100)");
        } 
        else if (c == 'S') {
            motors_stop_all();
            Serial.println("=> Motores PARADOS");
        }
        else if (c == 'E') {
            motor_esquerdo_set(100);
            motor_direito_set(0);
            Serial.println("=> Motor Esquerdo Girando");
        }
        else if (c == 'D') {
            motor_esquerdo_set(0);
            motor_direito_set(100);
            Serial.println("=> Motor Direito Girando");
        }
    }

    // A cada 300ms imprime o valor atual dos encoders
    if (millis() - ultimo_tempo_print > 300) {
        long cont_esq = encoder_esquerdo_get();
        long cont_dir = encoder_direito_get();
        
        Serial.printf("[ENCODERS] Esquerdo: %ld \t Direito: %ld\n", cont_esq, cont_dir);
        
        ultimo_tempo_print = millis();
    }
}