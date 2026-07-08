#ifndef CENTRALIZACAO_H
#define CENTRALIZACAO_H

#include <Arduino.h>

// ── Dimensões do labirinto ────────────────────────────
#define LARGURA_CELULA_MM       180   // Largura total da célula
#define ESPESSURA_PAREDE_MM      18   // Espessura de cada parede lateral
#define ESPACO_UTIL_MM          144   // Espaço livre entre as faces internas (180 - 18 - 18)
#define DIST_CENTRO_PAREDE_MM    72   // Distância ideal do centro até cada parede

// ── Limiares ──────────────────────────────────────────
#define LIMIAR_CORRECAO_MM       30   // 3cm: abaixo disso ativa correção agressiva
#define LIMIAR_PAREDE_LATERAL   130   // Abaixo disso considera que há parede (mesmo valor de distanceSensor.h)

// ── Ganhos do controlador PD ──────────────────────────
// ATENÇÃO: esses valores são iniciais e devem ser ajustados
//          empiricamente no labiri
// nto real!
#define KP_LATERAL             0.8f   // Ganho proporcional (erro lateral em mm → correção PWM)
#define KD_LATERAL             0.3f   // Ganho derivativo lateral
#define KP_GYRO                1.5f   // Ganho proporcional do giroscópio (erro em graus → correção PWM)
#define KD_GYRO                0.2f   // Ganho derivativo do giroscópio

// ── Limites de segurança ──────────────────────────────
#define CORRECAO_MAX            40    // Limite máximo de correção PWM (evita overcorrection)
#define FATOR_AGRESSIVO        1.5f   // Multiplicador quando dist <= LIMIAR_CORRECAO_MM

// ── Velocidade base para o modo centralizado ──────────
#define VEL_BASE_CENTRALIZADO  150    // Velocidade base dos motores durante navegação

/**
 * @brief Função principal de centralização. Deve ser chamada a cada ciclo do loop.
 *
 * Lê os sensores laterais (esq/dir), calcula o erro em relação ao centro
 * da célula e aplica correção diferencial nos motores.
 *
 * Cenários:
 *   - Ambas as paredes visíveis: erro = (distEsq - distDir) / 2
 *   - Apenas uma parede: erro = DIST_CENTRO - distância da parede presente
 *   - Sem paredes laterais: usa giroscópio (yaw) para manter reta
 *
 * Quando dist <= 30mm (3cm), aplica fator agressivo na correção.
 *
 * @param velocidade_base  Velocidade base dos motores (0–100 percentual).
 *                         Se 0, usa VEL_BASE_CENTRALIZADO.
 */
void centralizar_no_corredor(int velocidade_base = 0);

/**
 * @brief Habilita ou desabilita a centralização.
 *
 * Quando desabilitada, centralizar_no_corredor() não faz nada.
 * Útil durante giros e manobras especiais.
 */
void centralizar_habilitar(bool habilitar);

/**
 * @brief Retorna true se a centralização está habilitada.
 */
bool centralizar_esta_habilitado();

#endif // CENTRALIZACAO_H
