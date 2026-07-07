#ifndef MOVIMENTO_H
#define MOVIMENTO_H

#include <Arduino.h>

#define RODA_DIAMETRO_MM     32.0f
#define ENCODER_PPR          140
#define CELULA_MM            180.0f
#define DISTANCIA_EIXOS_MM   102.0f

#define VEL_PADRAO           150
#define VEL_GIRO             120

#define TIMEOUT_MS           3000

void mover_frente_celula();   // Anda exatamente 180 mm
void mover_tras_celula();     // Recua exatamente 180 mm
void girar_esquerda_90();     // Gira 90° no próprio eixo
void girar_direita_90();      // Gira 90° no próprio eixo

void mover_frente(int velocidade, unsigned long tempo_ms);
void mover_tras(int velocidade, unsigned long tempo_ms);
void girar_esquerda(int velocidade, unsigned long tempo_ms);
void girar_direita(int velocidade, unsigned long tempo_ms);

#endif
