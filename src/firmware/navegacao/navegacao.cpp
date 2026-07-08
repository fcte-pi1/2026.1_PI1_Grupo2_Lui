#include "navegacao.h"
#include "../distanceSensor/distanceSensor.h"
#include "../movimento/movimento.h"
#include "../motors/motors.h"

static uint8_t linha_atual   = 0;
static uint8_t coluna_atual  = 0;
static Direcao direcao_atual = NORTE;
static FaseNavegacao fase_atual = FASE_EXPLORANDO;

// Alvo do floodfill conforme a fase atual: 'S' na volta, 'C' nas outras.
static char alvo_da_fase(FaseNavegacao fase) {
    return (fase == FASE_VOLTANDO) ? 'S' : 'C';
}

// Converte "frente/direita/esquerda" (relativo ao robô) em direção absoluta
// (N/L/S/O), a partir da orientação atual — mesma conversão que o Hilmer faz
// em updateWalls() (navigation_controller.cpp), só que aqui via aritmética
// modular em vez de switch/case.
//   deslocamento: 0 = frente, 1 = direita, 3 = esquerda
static Direcao direcao_relativa(int deslocamento) {
    return (Direcao)(((int)direcao_atual + deslocamento + 4) % 4);
}

// Se a parede foi detectada e ainda não estava no mapa, grava e empilha a
// célula atual + a vizinha do outro lado da parede, para o floodfill
// incremental revalidar as distâncias ao redor (igual ao updateWalls() +
// push de fronteira do Hilmer). Retorna true se alguma parede era nova.
static bool registrar_parede_se_nova(uint8_t linha, uint8_t coluna, Direcao direcao, bool detectada) {
    if (!detectada) return false;                                  // não vimos parede nessa direção agora
    if (labirinto_tem_parede(linha, coluna, direcao)) return false; // já sabíamos dessa parede

    labirinto_definir_parede(linha, coluna, direcao, true);
    labirinto_fila_empilhar(linha, coluna);

    int dl = 0, dc = 0;
    switch (direcao) {
        case NORTE: dl =  1; break;
        case SUL:   dl = -1; break;
        case LESTE: dc =  1; break;
        case OESTE: dc = -1; break;
    }
    int vizinha_linha  = linha  + dl;
    int vizinha_coluna = coluna + dc;
    if (vizinha_linha >= 0 && vizinha_linha < TAMANHO_LABIRINTO &&
        vizinha_coluna >= 0 && vizinha_coluna < TAMANHO_LABIRINTO) {
        labirinto_fila_empilhar((uint8_t)vizinha_linha, (uint8_t)vizinha_coluna);
    }
    return true;
}

// Lê os 3 sensores de distância (relativos ao robô), registra paredes novas
// na célula atual e, se algo mudou, recalcula o floodfill incrementalmente.
static void atualizar_mapa_e_recalcular() {
    Direcao dir_frente   = direcao_relativa(0);
    Direcao dir_direita  = direcao_relativa(1);
    Direcao dir_esquerda = direcao_relativa(3);

    bool mudou = false;
    mudou |= registrar_parede_se_nova(linha_atual, coluna_atual, dir_frente,   tem_parede_frente());
    mudou |= registrar_parede_se_nova(linha_atual, coluna_atual, dir_direita,  tem_parede_direita());
    mudou |= registrar_parede_se_nova(linha_atual, coluna_atual, dir_esquerda, tem_parede_esquerda());

    if (mudou) {
        labirinto_recalcular_incremental(alvo_da_fase(fase_atual));
    }
}

// Gira fisicamente da orientação atual para a orientação alvo, escolhendo
// giro à direita, à esquerda ou meia-volta conforme a diferença angular.
static void girar_para(Direcao alvo) {
    int diferenca = ((int)alvo - (int)direcao_atual + 4) % 4;
    switch (diferenca) {
        case 0: break; // já está de frente pro alvo
        case 1: girar_direita_90(); break;
        case 2: girar_180();        break;
        case 3: girar_esquerda_90(); break;
    }
    direcao_atual = alvo;
}

// Anda exatamente 1 célula para frente e atualiza a posição lógica.
static void avancar_uma_celula() {
    mover_frente_celula();
    switch (direcao_atual) {
        case NORTE: linha_atual++;  break;
        case SUL:   linha_atual--;  break;
        case LESTE: coluna_atual++; break;
        case OESTE: coluna_atual--; break;
    }
}

void navegacao_init() {
    labirinto_init();
    linha_atual   = 0;
    coluna_atual  = 0;
    direcao_atual = NORTE;
    fase_atual    = FASE_EXPLORANDO;
    labirinto_recalcular_completo('C');
}

void navegacao_passo() {
    if (fase_atual == FASE_CONCLUIDA) return;

    // 1) atualiza o mapa com o que os sensores enxergam da célula atual
    atualizar_mapa_e_recalcular();

    // 2) verifica troca de fase (chegou no alvo da fase atual)
    if (fase_atual == FASE_EXPLORANDO && labirinto_eh_alvo(linha_atual, coluna_atual, 'C')) {
        fase_atual = FASE_VOLTANDO;
        labirinto_recalcular_completo('S');
    } else if (fase_atual == FASE_VOLTANDO && linha_atual == 0 && coluna_atual == 0) {
        fase_atual = FASE_CORRIDA;
        labirinto_recalcular_completo('C');
    } else if (fase_atual == FASE_CORRIDA && labirinto_eh_alvo(linha_atual, coluna_atual, 'C')) {
        fase_atual = FASE_CONCLUIDA;
        motors_stop_all();
        return;
    }

    // 3) escolhe, entre os vizinhos acessíveis, o de menor distância
    int melhor_distancia = DISTANCIA_INFINITA;
    Direcao melhor_direcao = direcao_atual;

    for (int d = 0; d < 4; d++) {
        Direcao dir = (Direcao)d;
        if (labirinto_tem_parede(linha_atual, coluna_atual, dir)) continue;

        int vizinha_linha = linha_atual, vizinha_coluna = coluna_atual;
        switch (dir) {
            case NORTE: vizinha_linha++;  break;
            case SUL:   vizinha_linha--;  break;
            case LESTE: vizinha_coluna++; break;
            case OESTE: vizinha_coluna--; break;
        }
        if (vizinha_linha < 0 || vizinha_linha >= TAMANHO_LABIRINTO ||
            vizinha_coluna < 0 || vizinha_coluna >= TAMANHO_LABIRINTO) continue;

        if (distancia[vizinha_linha][vizinha_coluna] < melhor_distancia) {
            melhor_distancia = distancia[vizinha_linha][vizinha_coluna];
            melhor_direcao   = dir;
        }
    }

    // 4) vira pra direção escolhida e avança 1 célula
    girar_para(melhor_direcao);
    avancar_uma_celula();
}

FaseNavegacao navegacao_fase()   { return fase_atual; }
uint8_t       navegacao_linha()  { return linha_atual; }
uint8_t       navegacao_coluna() { return coluna_atual; }
Direcao       navegacao_direcao() { return direcao_atual; }
