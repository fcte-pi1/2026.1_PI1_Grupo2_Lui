#include "movimento.h"
#include "../motors/motors.h"
#include "../encoder/encoder.h"
#include "../mpu/mpu.h"
#include "../distanceSensor/distanceSensor.h"
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

static bool atualizar_e_verificar_emergencia() {
    atualizar_filtro_media();
    return verificar_emergencia();
}

// ── Aguarda encoder atingir alvo ou timeout ───────────
static void aguardar_pulsos(long alvo_esq, long alvo_dir) {
    unsigned long inicio = millis();
    while (true) {
        if (atualizar_e_verificar_emergencia()) break;
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
    if (atualizar_e_verificar_emergencia()) return;
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(VEL_PADRAO);
    motor_direito_set(VEL_PADRAO);
    aguardar_pulsos(PULSOS_CELULA, PULSOS_CELULA);
}

// ── Mover 1 célula para trás (180 mm) ────────────────
void mover_tras_celula() {
    if (atualizar_e_verificar_emergencia()) return;
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
        if (atualizar_e_verificar_emergencia()) break;
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

// ── Ajuste de parede direita ──────────────────────────
// Se estiver a menos de DIST_MIN_DIREITA_MM (50 mm) da parede
// direita, gira à esquerda até a distância voltar a >= 50 mm.
void ajuste_parede_direita() {
    atualizar_filtro_media();
    if (verificar_emergencia()) return;
    if (distancia_direita_mm() >= DIST_MIN_DIREITA_MM) return;

    // Gira no próprio eixo para a esquerda
    motor_esquerdo_set(-VEL_AJUSTE);
    motor_direito_set(VEL_AJUSTE);

    unsigned long inicio = millis();
    while (distancia_direita_mm() < DIST_MIN_DIREITA_MM) {
        if (verificar_emergencia()) break;
        if (millis() - inicio > TIMEOUT_MS) break; // segurança
        atualizar_filtro_media(); // mantém a média do sensor atualizada
        delay(20);                // sensores leem a cada 20 ms
    }
    motors_stop_all();
}

// ── Ajuste de parede esquerda ─────────────────────────
// Se estiver a menos de DIST_MIN_ESQUERDA_MM (50 mm) da parede
// esquerda, gira à direita até a distância voltar a >= 50 mm.
void ajuste_parede_esquerda() {
    atualizar_filtro_media();
    if (verificar_emergencia()) return;
    if (distancia_esquerda_mm() >= DIST_MIN_ESQUERDA_MM) return;

    // Gira no próprio eixo para a direita
    motor_esquerdo_set(VEL_AJUSTE);
    motor_direito_set(-VEL_AJUSTE);

    unsigned long inicio = millis();
    while (distancia_esquerda_mm() < DIST_MIN_ESQUERDA_MM) {
        if (verificar_emergencia()) break;
        if (millis() - inicio > TIMEOUT_MS) break; // segurança
        atualizar_filtro_media(); // mantém a média do sensor atualizada
        delay(20);                // sensores leem a cada 20 ms
    }
    motors_stop_all();
}

static void recuar_ate_frente_segura() {
    motor_esquerdo_set(-VEL_AJUSTE);
    motor_direito_set(-VEL_AJUSTE);

    unsigned long inicio = millis();
    while (distancia_frente_mm() < LIMITE_REARME_EMERGENCIA_MM) {
        if (millis() - inicio > TIMEOUT_MS) break;
        atualizar_filtro_media();
        delay(20);
    }
    motors_stop_all();
}

static void girar_ate_lado_seguro(bool parede_esquerda) {
    if (parede_esquerda) {
        motor_esquerdo_set(VEL_AJUSTE);
        motor_direito_set(-VEL_AJUSTE);
    } else {
        motor_esquerdo_set(-VEL_AJUSTE);
        motor_direito_set(VEL_AJUSTE);
    }

    unsigned long inicio = millis();
    while (true) {
        atualizar_filtro_media();
        uint16_t distancia = parede_esquerda ? distancia_esquerda_mm() : distancia_direita_mm();
        if (distancia >= LIMITE_REARME_EMERGENCIA_MM) break;
        if (distancia_frente_mm() <= LIMITE_EMERGENCIA_TOF_MM) break;
        if (millis() - inicio > TIMEOUT_MS) break;
        delay(20);
    }
    motors_stop_all();
}

void recuperar_centro_labirinto() {
    atualizar_filtro_media();
    if (!verificar_emergencia()) return;

    uint8_t lados = emergencia_tof_lados();
    motors_stop_all();
    delay(150);

    if (lados & EMERGENCIA_TOF_FRENTE) {
        recuar_ate_frente_segura();
    }
    if (lados & EMERGENCIA_TOF_ESQ) {
        girar_ate_lado_seguro(true);
    }
    if (lados & EMERGENCIA_TOF_DIR) {
        girar_ate_lado_seguro(false);
    }

    atualizar_filtro_media();
    limpar_emergencia_se_seguro();
    motors_stop_all();
}

// ── Funções legadas por tempo ─────────────────────────
void mover_frente(int velocidade, unsigned long tempo_ms) {
    if (atualizar_e_verificar_emergencia()) return;
    motor_esquerdo_set(velocidade);
    motor_direito_set(velocidade);
    unsigned long inicio = millis();
    while (millis() - inicio < tempo_ms) {
        if (atualizar_e_verificar_emergencia()) break;
        delay(10);
    }
    motors_stop_all();
}

void mover_tras(int velocidade, unsigned long tempo_ms) {
    if (atualizar_e_verificar_emergencia()) return;
    motor_esquerdo_set(-velocidade);
    motor_direito_set(-velocidade);
    unsigned long inicio = millis();
    while (millis() - inicio < tempo_ms) {
        if (atualizar_e_verificar_emergencia()) break;
        delay(10);
    }
    motors_stop_all();
}

void girar_esquerda(int velocidade, unsigned long tempo_ms) {
    if (atualizar_e_verificar_emergencia()) return;
    motor_esquerdo_set(-velocidade);
    motor_direito_set(velocidade);
    unsigned long inicio = millis();
    while (millis() - inicio < tempo_ms) {
        if (atualizar_e_verificar_emergencia()) break;
        delay(10);
    }
    motors_stop_all();
}

void girar_direita(int velocidade, unsigned long tempo_ms) {
    if (atualizar_e_verificar_emergencia()) return;
    motor_esquerdo_set(velocidade);
    motor_direito_set(-velocidade);
    unsigned long inicio = millis();
    while (millis() - inicio < tempo_ms) {
        if (atualizar_e_verificar_emergencia()) break;
        delay(10);
    }
    motors_stop_all();
}
