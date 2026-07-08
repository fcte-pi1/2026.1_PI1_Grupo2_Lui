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

#define DIST_MIN_DIREITA_MM  50    // Distância mínima da parede direita
#define DIST_MIN_ESQUERDA_MM 50    // Distância mínima da parede esquerda
#define VEL_AJUSTE           100   // Velocidade do giro de ajuste

void mover_frente_celula();   // Anda exatamente 180 mm
void mover_tras_celula();     // Recua exatamente 180 mm
void girar_esquerda_90();     // Gira 90° no próprio eixo
void girar_direita_90();      // Gira 90° no próprio eixo
void girar_180();             // Meia-volta no próprio eixo
void ajuste_parede_direita();  // Se dir < 50 mm, gira à esquerda até dir >= 50 mm
void ajuste_parede_esquerda(); // Se esq < 50 mm, gira à direita até esq >= 50 mm
void recuperar_centro_labirinto(); // Afasta o robo da parede apos emergency stop

void mover_frente(int velocidade, unsigned long tempo_ms);
void mover_tras(int velocidade, unsigned long tempo_ms);
void girar_esquerda(int velocidade, unsigned long tempo_ms);
void girar_direita(int velocidade, unsigned long tempo_ms);

#endif
