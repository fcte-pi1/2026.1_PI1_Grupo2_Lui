#ifndef NAVEGACAO_H
#define NAVEGACAO_H

#include "../labirinto/labirinto.h"

// Máquina de estados de navegação, adaptada do laço de 3 fases do grupo
// 2026_1_PI1_Grupo01_Hilmer (src/backend/FloodFill.cpp): ida ao centro ->
// volta ao início -> corrida final pelo caminho ótimo. Ver
// ~/Documentos/logica_hilmer_referencia.cpp para o material original.
enum FaseNavegacao : uint8_t {
    FASE_EXPLORANDO = 0,  // (0,0) -> bloco central, mapeando o labirinto
    FASE_VOLTANDO,        // bloco central -> (0,0), com o mapa já conhecido
    FASE_CORRIDA,         // (0,0) -> bloco central, percurso final otimizado
    FASE_CONCLUIDA,
};

// Zera posição/orientação lógica, inicializa o labirinto e calcula o
// primeiro floodfill (alvo = centro). Chamar uma vez antes de navegacao_passo().
void navegacao_init();

// Executa 1 passo da navegação: lê paredes, atualiza o mapa (recalculando o
// floodfill se necessário), decide a direção de menor distância, gira e anda
// 1 célula. Chamar repetidamente (ex.: a cada iteração do loop()) até
// navegacao_fase() == FASE_CONCLUIDA.
void navegacao_passo();

FaseNavegacao navegacao_fase();
uint8_t       navegacao_linha();
uint8_t       navegacao_coluna();
Direcao       navegacao_direcao();

#endif
