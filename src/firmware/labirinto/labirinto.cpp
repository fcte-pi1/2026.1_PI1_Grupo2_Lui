#include "labirinto.h"
#include <string.h>

uint8_t distancia[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO];
bool parede_horizontal[TAMANHO_LABIRINTO + 1][TAMANHO_LABIRINTO];
bool parede_vertical[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO + 1];

// ── Fila circular do BFS (igual ao floodfill_engine.cpp do Hilmer) ──────
// Bitmask "na_fila" evita empilhar a mesma célula duas vezes.
#define FILA_MAX 256 // precisa ser potência de 2 (usa & em vez de %)

typedef struct {
    uint8_t linha;
    uint8_t coluna;
} CelulaFila;

static uint32_t na_fila[TAMANHO_LABIRINTO];
static CelulaFila fila[FILA_MAX];
static int fila_inicio = 0;
static int fila_fim = 0;

void labirinto_reset_fila() {
    fila_inicio = 0;
    fila_fim = 0;
    memset(na_fila, 0, sizeof(na_fila));
}

bool labirinto_fila_empilhar(uint8_t linha, uint8_t coluna) {
    if (na_fila[linha] & (1u << coluna))
        return false; // já está na fila
    if (((fila_fim + 1) & (FILA_MAX - 1)) == fila_inicio)
        return false; // fila cheia

    na_fila[linha] |= (1u << coluna);
    fila[fila_fim].linha = linha;
    fila[fila_fim].coluna = coluna;
    fila_fim = (fila_fim + 1) & (FILA_MAX - 1);
    return true;
}

static CelulaFila labirinto_fila_desempilhar() {
    CelulaFila c = fila[fila_inicio];
    na_fila[c.linha] &= ~(1u << c.coluna);
    fila_inicio = (fila_inicio + 1) & (FILA_MAX - 1);
    return c;
}

static bool labirinto_fila_vazia() {
    return fila_inicio == fila_fim;
}

// ── Inicialização ─────────────────────────────────────────────────────

void labirinto_init() {
    memset(distancia, DISTANCIA_INFINITA, sizeof(distancia));
    memset(parede_horizontal, 0, sizeof(parede_horizontal));
    memset(parede_vertical, 0, sizeof(parede_vertical));

    // Paredes externas (bordas do labirinto)
    for (int i = 0; i < TAMANHO_LABIRINTO; i++) {
        parede_horizontal[0][i]                 = true;
        parede_horizontal[TAMANHO_LABIRINTO][i] = true;
        parede_vertical[i][0]                   = true;
        parede_vertical[i][TAMANHO_LABIRINTO]   = true;
    }
}

// ── Acesso a paredes por direção absoluta ────────────────────────────

void labirinto_definir_parede(uint8_t linha, uint8_t coluna, Direcao direcao, bool tem_parede) {
    switch (direcao) {
        case NORTE: parede_horizontal[linha + 1][coluna] = tem_parede; break;
        case SUL:   parede_horizontal[linha][coluna]     = tem_parede; break;
        case LESTE: parede_vertical[linha][coluna + 1]   = tem_parede; break;
        case OESTE: parede_vertical[linha][coluna]       = tem_parede; break;
    }
}

bool labirinto_tem_parede(uint8_t linha, uint8_t coluna, Direcao direcao) {
    switch (direcao) {
        case NORTE: return parede_horizontal[linha + 1][coluna];
        case SUL:   return parede_horizontal[linha][coluna];
        case LESTE: return parede_vertical[linha][coluna + 1];
        case OESTE: return parede_vertical[linha][coluna];
    }
    return true;
}

bool labirinto_eh_alvo(int linha, int coluna, char alvo) {
    if (alvo == 'S')
        return (linha == 0 && coluna == 0);
    if (alvo == 'C') {
        int meio = TAMANHO_LABIRINTO / 2;
        return ((linha == meio - 1 || linha == meio) && (coluna == meio - 1 || coluna == meio));
    }
    return false;
}

static int labirinto_menor_vizinho(int linha, int coluna) {
    int menor = DISTANCIA_INFINITA;
    if (!parede_horizontal[linha + 1][coluna] && distancia[linha + 1][coluna] < menor) menor = distancia[linha + 1][coluna];
    if (!parede_horizontal[linha][coluna]     && distancia[linha - 1][coluna] < menor) menor = distancia[linha - 1][coluna];
    if (!parede_vertical[linha][coluna + 1]   && distancia[linha][coluna + 1] < menor) menor = distancia[linha][coluna + 1];
    if (!parede_vertical[linha][coluna]       && distancia[linha][coluna - 1] < menor) menor = distancia[linha][coluna - 1];
    return menor;
}

// ── Recalculo completo (O(N²)) — usar ao trocar de alvo/fase ─────────

void labirinto_recalcular_completo(char alvo) {
    memset(distancia, DISTANCIA_INFINITA, sizeof(distancia));
    labirinto_reset_fila();

    if (alvo == 'C') {
        int meio = TAMANHO_LABIRINTO / 2;
        distancia[meio - 1][meio - 1] = 0; labirinto_fila_empilhar((uint8_t)(meio - 1), (uint8_t)(meio - 1));
        distancia[meio - 1][meio]     = 0; labirinto_fila_empilhar((uint8_t)(meio - 1), (uint8_t)meio);
        distancia[meio][meio - 1]     = 0; labirinto_fila_empilhar((uint8_t)meio, (uint8_t)(meio - 1));
        distancia[meio][meio]         = 0; labirinto_fila_empilhar((uint8_t)meio, (uint8_t)meio);
    } else if (alvo == 'S') {
        distancia[0][0] = 0;
        labirinto_fila_empilhar(0, 0);
    }

    while (!labirinto_fila_vazia()) {
        CelulaFila atual = labirinto_fila_desempilhar();
        int linha = atual.linha;
        int coluna = atual.coluna;
        int valor_atual = distancia[linha][coluna];

        if (!parede_horizontal[linha + 1][coluna] && distancia[linha + 1][coluna] == DISTANCIA_INFINITA) {
            distancia[linha + 1][coluna] = valor_atual + 1;
            labirinto_fila_empilhar((uint8_t)(linha + 1), (uint8_t)coluna);
        }
        if (!parede_horizontal[linha][coluna] && distancia[linha - 1][coluna] == DISTANCIA_INFINITA) {
            distancia[linha - 1][coluna] = valor_atual + 1;
            labirinto_fila_empilhar((uint8_t)(linha - 1), (uint8_t)coluna);
        }
        if (!parede_vertical[linha][coluna + 1] && distancia[linha][coluna + 1] == DISTANCIA_INFINITA) {
            distancia[linha][coluna + 1] = valor_atual + 1;
            labirinto_fila_empilhar((uint8_t)linha, (uint8_t)(coluna + 1));
        }
        if (!parede_vertical[linha][coluna] && distancia[linha][coluna - 1] == DISTANCIA_INFINITA) {
            distancia[linha][coluna - 1] = valor_atual + 1;
            labirinto_fila_empilhar((uint8_t)linha, (uint8_t)(coluna - 1));
        }
    }
}

// ── Recalculo incremental (O(K)) — usar ao descobrir parede nova ─────
// Pré-requisito: quem descobriu a parede já empilhou as células afetadas.

void labirinto_recalcular_incremental(char alvo) {
    while (!labirinto_fila_vazia()) {
        CelulaFila atual = labirinto_fila_desempilhar();
        int linha = atual.linha;
        int coluna = atual.coluna;

        if (labirinto_eh_alvo(linha, coluna, alvo))
            continue; // alvo sempre tem distância 0, não mexe

        int menor_vizinho = labirinto_menor_vizinho(linha, coluna);

        if (distancia[linha][coluna] != menor_vizinho + 1) {
            distancia[linha][coluna] = (uint8_t)(menor_vizinho + 1);

            if (!parede_horizontal[linha + 1][coluna]) labirinto_fila_empilhar((uint8_t)(linha + 1), (uint8_t)coluna);
            if (!parede_horizontal[linha][coluna])     labirinto_fila_empilhar((uint8_t)(linha - 1), (uint8_t)coluna);
            if (!parede_vertical[linha][coluna + 1])   labirinto_fila_empilhar((uint8_t)linha, (uint8_t)(coluna + 1));
            if (!parede_vertical[linha][coluna])       labirinto_fila_empilhar((uint8_t)linha, (uint8_t)(coluna - 1));
        }
    }
}
