#include "movimento.h"
#include "../motors/motors.h"
#include "../encoder/encoder.h"
#include "../pid/pid.h"
#include "../distanceSensor/distanceSensor.h"
#include "../mpu/mpu.h"
#include <math.h>

// ── Cálculos derivados das constantes ────────────────
// Circunferência da roda: π × 32 ≈ 100.53 mm
static const float CIRCUNFERENCIA_MM = M_PI * RODA_DIAMETRO_MM;

// Pulsos por mm: 20 / 100.53 ≈ 0.199 pulsos/mm
static const float PULSOS_POR_MM = (float)ENCODER_PPR / CIRCUNFERENCIA_MM;

// Pulsos para 1 célula (180 mm): 180 × 0.199 ≈ 36 pulsos
static const long PULSOS_CELULA = (long)(CELULA_MM * PULSOS_POR_MM + 0.5f);

// Pulsos para girar 90°:
// Arco de cada roda = π × 102 / 4 ≈ 80.1 mm → 80.1 × 0.199 ≈ 16 pulsos
static const long PULSOS_GIRO_90 = (long)((M_PI * DISTANCIA_EIXOS_MM / 4.0f) * PULSOS_POR_MM + 0.5f);

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

// Mover 1 célula para frente (180 mm) com controle PID
void avancar_celula() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    resetar_pid();
    mpu_reset_yaw();

    unsigned long inicio = millis();
    unsigned long ultimo_tempo_pid = inicio;

    while (true) {
        long esq = abs(encoder_esquerdo_get());
        long dir = abs(encoder_direito_get());

        if (esq >= PULSOS_CELULA && dir >= PULSOS_CELULA) break;
        if (millis() - inicio > TIMEOUT_MS) break;

        // Atualizar leituras de hardware
        atualizar_filtro_media();
        mpu_update();

        unsigned long agora = millis();
        float dt = (agora - ultimo_tempo_pid) / 1000.0f;
        ultimo_tempo_pid = agora;

        float erro = obter_erro_centralizacao();
        float ajuste = calcular_pid(erro, dt);

        // Aplicar PID:
        // Erro positivo = muito na esquerda. Precisamos virar à direita.
        // Virar à direita = roda esquerda gira mais rápido, direita mais devagar.
        int vel_esq = VEL_PADRAO + (int)ajuste;
        int vel_dir = VEL_PADRAO - (int)ajuste;

        // Limites do PWM (0 a 255)
        if (vel_esq > 255) vel_esq = 255;
        if (vel_esq < 0) vel_esq = 0;
        if (vel_dir > 255) vel_dir = 255;
        if (vel_dir < 0) vel_dir = 0;

        motor_esquerdo_set(vel_esq);
        motor_direito_set(vel_dir);

        delay(5); // Pequeno atraso para não saturar a CPU
    }
    motors_stop_all();
}

// ── Mover 1 célula para trás (180 mm) ────────────────
void mover_tras_celula() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(-VEL_PADRAO);
    motor_direito_set(-VEL_PADRAO);
    aguardar_pulsos(PULSOS_CELULA, PULSOS_CELULA);
}

// ── Girar 90° para a esquerda (no próprio eixo) ──────
void girar_esquerda_90() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(-VEL_GIRO);
    motor_direito_set(VEL_GIRO);
    aguardar_pulsos(PULSOS_GIRO_90, PULSOS_GIRO_90);
}

// ── Girar 90° para a direita (no próprio eixo) ───────
void girar_direita_90() {
    encoder_esquerdo_reset();
    encoder_direito_reset();
    motor_esquerdo_set(VEL_GIRO);
    motor_direito_set(-VEL_GIRO);
    aguardar_pulsos(PULSOS_GIRO_90, PULSOS_GIRO_90);
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

// ── Funções de Velocidade ─────────────────────────────

static float velocidade_atual_mm_s = 0.0f;
static long pulsos_esq_ant = 0;
static long pulsos_dir_ant = 0;
static unsigned long ultimo_tempo_vel = 0;

void atualizar_velocidade() {
    unsigned long agora = millis();
    
    // Na primeira execução, apenas salva o estado atual
    if (ultimo_tempo_vel == 0) {
        ultimo_tempo_vel = agora;
        pulsos_esq_ant = abs(encoder_esquerdo_get());
        pulsos_dir_ant = abs(encoder_direito_get());
        return;
    }
    
    float dt = (agora - ultimo_tempo_vel) / 1000.0f; // tempo em segundos
    
    // Atualiza apenas a cada 50ms para ter precisão e evitar flutuação
    if (dt < 0.05f) return; 
    
    long esq = abs(encoder_esquerdo_get());
    long dir = abs(encoder_direito_get());
    
    long delta_esq = esq - pulsos_esq_ant;
    long delta_dir = dir - pulsos_dir_ant;
    
    // Se o robô andou para trás (delta negativo), abs previne velocidade negativa estranha.
    // Usaremos a velocidade escalar média (módulo do deslocamento)
    float mm_esq = abs(delta_esq) / PULSOS_POR_MM;
    float mm_dir = abs(delta_dir) / PULSOS_POR_MM;
    
    // Média da velocidade linear das duas rodas
    velocidade_atual_mm_s = ((mm_esq + mm_dir) / 2.0f) / dt;
    
    pulsos_esq_ant = esq;
    pulsos_dir_ant = dir;
    ultimo_tempo_vel = agora;
}

float obter_velocidade_mm_s() {
    return velocidade_atual_mm_s;
}
