#include "movimento.h"
#include "../motors/motors.h"
#include "../encoder/encoder.h"
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
    unsigned long ultimo_print = 0;
    
    Serial.printf("[MOV] Iniciando aguardar_pulsos. Alvo Esq: %ld, Alvo Dir: %ld\n", alvo_esq, alvo_dir);
    
    while (true) {
        long raw_esq = encoder_esquerdo_get();
        long raw_dir = encoder_direito_get();
        long esq = abs(raw_esq);
        long dir = abs(raw_dir);
        
        if (millis() - ultimo_print >= 100) {
            ultimo_print = millis();
            Serial.printf("[MOV] Ticks - Esq: %ld (abs: %ld) | Dir: %ld (abs: %ld)\n", raw_esq, esq, raw_dir, dir);
        }
        
        if (esq >= alvo_esq && dir >= alvo_dir) {
            Serial.printf("[MOV] Alvo atingido! Esq: %ld/%ld, Dir: %ld/%ld\n", esq, alvo_esq, dir, alvo_dir);
            break;
        }
        if (millis() - inicio > TIMEOUT_MS) {
            Serial.printf("[MOV] TIMEOUT! Esq: %ld/%ld, Dir: %ld/%ld\n", esq, alvo_esq, dir, alvo_dir);
            break;
        }
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
