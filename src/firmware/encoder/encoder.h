#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// PPR agora é configurado centralmente em movimento.h

// ── Inicialização ─────────────────────────────────────
void encoders_init();

// ── Leitura de contagem acumulada (thread-safe) ───────
long encoder_esquerdo_get();
long encoder_direito_get();

// ── Zera os contadores ────────────────────────────────
void encoder_esquerdo_reset();
void encoder_direito_reset();

// ── Sentido de rotação: +1 = frente, -1 = ré ─────────
int encoder_esquerdo_sentido();
int encoder_direito_sentido();

#endif // ENCODER_H
