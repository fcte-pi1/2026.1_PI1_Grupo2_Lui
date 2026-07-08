#ifndef LABIRINTO_H
#define LABIRINTO_H

#include <Arduino.h>

// Lógica portada do floodfill do grupo 2026_1_PI1_Grupo01_Hilmer
// (src/firmware/micromouse/src/navigation/floodfill_engine.cpp), adaptada
// para os nomes/variáveis deste projeto. Ver ~/Documentos/logica_hilmer_referencia.cpp
// para o material original de referência.

#define TAMANHO_LABIRINTO   16      // 16x16 células de CELULA_MM (ver movimento.h) = 2,88 m de lado
#define DISTANCIA_INFINITA  255     // célula ainda não alcançada pelo BFS

// Direções cardeais absolutas (não confundir com "frente/direita/esquerda",
// que são relativas à orientação atual do robô — essa conversão é feita em
// navegacao.cpp). NORTE aumenta a linha, LESTE aumenta a coluna.
enum Direcao : uint8_t {
    NORTE = 0,
    LESTE = 1,
    SUL   = 2,
    OESTE = 3,
};

// distancia[linha][coluna]: valor do BFS (DISTANCIA_INFINITA = não calculado)
extern uint8_t distancia[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO];

// Paredes como segmentos compartilhados entre células vizinhas (não como 4
// bools por célula) — assim marcar uma parede já vale automaticamente para
// os dois lados, sem precisar propagar pra célula vizinha.
// parede_horizontal[linha][coluna] = parede ao SUL da célula (linha, coluna)
//                                    (equivale à parede ao NORTE de (linha-1, coluna))
// parede_vertical[linha][coluna]   = parede a OESTE da célula (linha, coluna)
extern bool parede_horizontal[TAMANHO_LABIRINTO + 1][TAMANHO_LABIRINTO];
extern bool parede_vertical[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO + 1];

// Zera o mapa (distâncias, paredes) e marca as paredes externas do labirinto.
void labirinto_init();

// Consulta/gravação de parede por direção absoluta, cuidando da indexação
// correta do segmento compartilhado.
void labirinto_definir_parede(uint8_t linha, uint8_t coluna, Direcao direcao, bool tem_parede);
bool labirinto_tem_parede(uint8_t linha, uint8_t coluna, Direcao direcao);

// Alvo: 'C' = bloco central 2x2, 'S' = início (0,0)
bool labirinto_eh_alvo(int linha, int coluna, char alvo);

// Fila usada pelo BFS (exposta para quem descobre paredes novas poder
// empilhar diretamente as células afetadas antes de chamar o recálculo
// incremental — mesmo padrão do Hilmer).
void labirinto_reset_fila();
bool labirinto_fila_empilhar(uint8_t linha, uint8_t coluna);

// Recalcula TODAS as distâncias via BFS (O(N²)). Usar ao trocar de alvo/fase
// (ex.: trocar de "ir pro centro" para "voltar ao início").
void labirinto_recalcular_completo(char alvo);

// Recalcula só o necessário (O(K), K = nº de células afetadas), quando uma
// parede nova é descoberta no meio do percurso. Pré-requisito: as células
// afetadas já devem ter sido empilhadas com labirinto_fila_empilhar() antes.
void labirinto_recalcular_incremental(char alvo);

#endif
