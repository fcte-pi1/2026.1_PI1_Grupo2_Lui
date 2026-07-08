#include "movimento.h"
#include "../motors/motors.h"
#include "../encoder/encoder.h"
#include "../mpu/mpu.h"
#include <math.h>

// ── Cálculos derivados das constantes ────────────────
// Circunferência da roda: π × 32 ≈ 100.53 mm
static const float CIRCUNFERENCIA_MM = M_PI * RODA_DIAMETRO_MM;

// Pulsos por mm: 20 / 100.53 ≈ 0.199 pulsos/mm
static const float PULSOS_POR_MM = (float)ENCODER_PPR / CIRCUNFERENCIA_MM;

// Pulsos para 1 célula (180 mm): 180 × 0.199 ≈ 36 pulsos
static const long PULSOS_CELULA = (long)(CELULA_MM * PULSOS_POR_MM + 0.5f);

// Giro de 90° é feito com o giroscópio (ver girar_com_giroscopio), não por
// pulsos de encoder: a contagem de pulsos depende da geometria do eixo e de
// ambos os encoders estarem saudáveis, enquanto o giroscópio mede a rotação
// real do robô diretamente.
static const float ANGULO_GIRO_90_GRAUS = 90.0f;

// Faixa final do giro (graus faltando para o alvo) em que a velocidade é
// reduzida à metade, para frear antes de bater no ângulo e evitar overshoot
// por inércia do robô.
static const float ZONA_FRENAGEM_GRAUS = 20.0f;

// ── Aguarda encoder atingir alvo ou timeout ───────────
static void aguardar_pulsos(long alvo_esq, long alvo_dir) {
    unsigned long inicio = millis();
    while (true) {
        long esq = abs(encoder_esquerdo_get());
        long dir = abs(encoder_direito_get());
        if (esq >= alvo_esq && dir >= alvo_dir) break;
        if (millis() - inicio > TIMEOUT_MS) break;
        delay(1);
    }
    motors_stop_all();
}

// ── Mover 1 célula para frente (180 mm) ──────────────
void mover_frente_celula() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(VEL_PADRAO);
    motor_direito_set(VEL_PADRAO);
    aguardar_pulsos(PULSOS_CELULA, PULSOS_CELULA);
}

// ── Mover 1 célula para trás (180 mm) ────────────────
void mover_tras_celula() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(-VEL_PADRAO);
    motor_direito_set(-VEL_PADRAO);
    aguardar_pulsos(PULSOS_CELULA, PULSOS_CELULA);
}

// ── Gira no próprio eixo até o giroscópio acusar o ângulo alvo ────────
// velocidade_esq/velocidade_dir definem o sentido do giro (sinais opostos).
// O ângulo é integrado a partir da velocidade angular do giroscópio (ver
// mpu_atualizar_angulo), zerado no início e conferido a cada iteração.
static void girar_com_giroscopio(int velocidade_esq, int velocidade_dir, float alvo_graus) {
    mpu_zerar_angulo();
    motor_esquerdo_set(velocidade_esq);
    motor_direito_set(velocidade_dir);

    unsigned long inicio = millis();
    bool freando = false;

    while (true) {
        mpu_atualizar_angulo();
        float atual_graus = fabsf(mpu_get_angulo());

        if (atual_graus >= alvo_graus) break;
        if (millis() - inicio > TIMEOUT_MS) break;

        // Perto do alvo, reduz a velocidade pela metade para nao passar do
        // ponto por causa da inercia do robo.
        if (!freando && atual_graus >= alvo_graus - ZONA_FRENAGEM_GRAUS) {
            freando = true;
            motor_esquerdo_set(velocidade_esq / 2);
            motor_direito_set(velocidade_dir / 2);
        }
        delay(2);
    }
    motors_stop_all();
}

// ── Girar 90° para a esquerda (no próprio eixo) ──────
void girar_esquerda_90() {
    girar_com_giroscopio(-VEL_GIRO, VEL_GIRO, ANGULO_GIRO_90_GRAUS);
}

// ── Girar 90° para a direita (no próprio eixo) ───────
void girar_direita_90() {
    girar_com_giroscopio(VEL_GIRO, -VEL_GIRO, ANGULO_GIRO_90_GRAUS);
}

void girar_180() {
    girar_com_giroscopio(VEL_GIRO, -VEL_GIRO, 180.0f);
}

// ── Funções legadas por tempo ─────────────────────────
void mover_frente(int velocidade, unsigned long tempo_ms) {
    motor_esquerdo_set(velocidade);
    motor_direito_set(velocidade);
    delay(tempo_ms);
    motors_stop_all();
}

void mover_tras(int velocidade, unsigned long tempo_ms) {
    motor_esquerdo_set(-velocidade);
    motor_direito_set(-velocidade);
    delay(tempo_ms);
    motors_stop_all();
}

void girar_esquerda(int velocidade, unsigned long tempo_ms) {
    motor_esquerdo_set(-velocidade);
    motor_direito_set(velocidade);
    delay(tempo_ms);
    motors_stop_all();
}

void girar_direita(int velocidade, unsigned long tempo_ms) {
    motor_esquerdo_set(velocidade);
    motor_direito_set(-velocidade);
    delay(tempo_ms);
    motors_stop_all();
}
