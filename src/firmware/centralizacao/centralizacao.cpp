#include "centralizacao.h"
#include "../distanceSensor/distanceSensor.h"
#include "../mpu/mpu.h"
#include "../motors/motors.h"
#include <math.h>

// ── Estado interno ────────────────────────────────────
static bool habilitado = true;
static float erro_anterior = 0.0f;
static float erro_gyro_anterior = 0.0f;
static unsigned long ultimo_tempo_centralizacao = 0;

// ── Habilitar / Desabilitar ───────────────────────────
void centralizar_habilitar(bool habilitar) {
    habilitado = habilitar;
    if (habilitar) {
        // Reset dos estados ao reabilitar
        erro_anterior = 0.0f;
        erro_gyro_anterior = 0.0f;
        ultimo_tempo_centralizacao = millis();
        resetarYaw();  // Zera o yaw para o novo trecho reto
    }
}

bool centralizar_esta_habilitado() {
    return habilitado;
}

// ── Função principal de centralização ─────────────────
void centralizar_no_corredor(int velocidade_base) {

    if (!habilitado) return;

    // Velocidade base padrão
    if (velocidade_base <= 0) velocidade_base = VEL_BASE_CENTRALIZADO;

    // ── 1. Ler distâncias filtradas ───────────────────
    uint16_t distEsq = obterDistanciaEsq();
    uint16_t distDir = obterDistanciaDir();

    bool paredeEsq = (distEsq < LIMIAR_PAREDE_LATERAL);
    bool paredeDir = (distDir < LIMIAR_PAREDE_LATERAL);

    // ── 2. Calcular Δt ────────────────────────────────
    unsigned long agora = millis();
    float dt = (float)(agora - ultimo_tempo_centralizacao) / 1000.0f;
    ultimo_tempo_centralizacao = agora;

    // Proteção contra dt inválido
    if (dt <= 0.0f || dt > 0.5f) {
        dt = 0.02f;  // Fallback: 20ms (frequência esperada do loop)
    }

    // ── 3. Calcular erro lateral ──────────────────────
    //   erro > 0: robô está deslocado para a DIREITA  → precisa ir para esquerda
    //   erro < 0: robô está deslocado para a ESQUERDA → precisa ir para direita
    float erro_lateral = 0.0f;
    bool usar_gyro_apenas = false;

    if (paredeEsq && paredeDir) {
        // Caso ideal: ambas as paredes visíveis
        // erro = (distEsq - distDir) / 2
        // Se distEsq > distDir → robô está mais perto da dir → erro positivo
        erro_lateral = ((float)distEsq - (float)distDir) / 2.0f;
    }
    else if (paredeEsq && !paredeDir) {
        // Só parede esquerda: erro = distância real - distância ideal
        // Se distEsq < 72 → robô está muito perto da esq → erro negativo → corrigir para dir
        erro_lateral = (float)distEsq - (float)DIST_CENTRO_PAREDE_MM;
        // Inverter: se distEsq < 72, queremos ir para direita (erro negativo)
        // já está correto: distEsq=40 → erro=-32 → corrige para direita
    }
    else if (!paredeEsq && paredeDir) {
        // Só parede direita: erro = distância ideal - distância real
        // Se distDir < 72 → robô está muito perto da dir → erro positivo → corrigir para esq
        erro_lateral = (float)DIST_CENTRO_PAREDE_MM - (float)distDir;
        // distDir=40 → erro=+32 → corrige para esquerda
    }
    else {
        // Sem paredes laterais: usar apenas giroscópio
        usar_gyro_apenas = true;
    }

    // ── 4. Verificar limiar de 3cm (correção agressiva) ──
    float fator_correcao = 1.0f;
    if (!usar_gyro_apenas) {
        if ((paredeEsq && distEsq <= LIMIAR_CORRECAO_MM) ||
            (paredeDir && distDir <= LIMIAR_CORRECAO_MM)) {
            fator_correcao = FATOR_AGRESSIVO;
            Serial.printf("[CENTR] ALERTA! Dist perigosa - Esq:%d Dir:%d mm\n", distEsq, distDir);
        }
    }

    // ── 5. Controlador PD lateral ─────────────────────
    float derivada_lateral = (erro_lateral - erro_anterior) / dt;
    erro_anterior = erro_lateral;

    float correcao_lateral = 0.0f;
    if (!usar_gyro_apenas) {
        correcao_lateral = (KP_LATERAL * erro_lateral + KD_LATERAL * derivada_lateral) * fator_correcao;
    }

    // ── 6. Controlador PD do giroscópio ───────────────
    //   yaw > 0: robô girou para a esquerda → corrigir para direita
    //   yaw < 0: robô girou para a direita  → corrigir para esquerda
    float yaw = obterYaw();
    float derivada_gyro = (yaw - erro_gyro_anterior) / dt;
    erro_gyro_anterior = yaw;

    float correcao_gyro = KP_GYRO * yaw + KD_GYRO * derivada_gyro;

    // ── 7. Combinar correções ─────────────────────────
    float correcao_total;
    if (usar_gyro_apenas) {
        // Sem paredes: depende inteiramente do giroscópio
        correcao_total = correcao_gyro;
    } else {
        // Com paredes: prioriza sensores de distância, gyro como complemento
        correcao_total = correcao_lateral + 0.3f * correcao_gyro;
    }

    // ── 8. Limitar correção ───────────────────────────
    if (correcao_total > CORRECAO_MAX)  correcao_total = CORRECAO_MAX;
    if (correcao_total < -CORRECAO_MAX) correcao_total = -CORRECAO_MAX;

    // ── 9. Aplicar nos motores ────────────────────────
    //   correcao_total > 0: robô deslocado para direita → reduz motor dir, aumenta motor esq
    //   correcao_total < 0: robô deslocado para esquerda → reduz motor esq, aumenta motor dir
    int vel_esq = velocidade_base - (int)correcao_total;
    int vel_dir = velocidade_base + (int)correcao_total;

    // Garante que as velocidades não ultrapassam os limites
    vel_esq = constrain(vel_esq, 0, 100);
    vel_dir = constrain(vel_dir, 0, 100);

    motor_esquerdo_set(vel_esq);
    motor_direito_set(vel_dir);

    // ── 10. Log periódico para debug ──────────────────
    static unsigned long ultimo_log = 0;
    if (agora - ultimo_log >= 200) {  // Log a cada 200ms
        ultimo_log = agora;
        Serial.printf("[CENTR] Esq:%3d Dir:%3d | Erro:%.1f Gyro:%.1f | Corr:%.1f | Vel E:%d D:%d\n",
                      distEsq, distDir, erro_lateral, yaw, correcao_total, vel_esq, vel_dir);
    }
}
