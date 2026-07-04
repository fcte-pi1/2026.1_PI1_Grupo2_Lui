#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#include <Arduino.h>

#define MAZE_SIZE 16

enum Direction {
    NORTE = 0,
    LESTE = 1,
    SUL   = 2,
    OESTE = 3
};

enum MoveAction {
    MOVE_FRENTE = 0,
    MOVE_DIREITA = 1,
    MOVE_MEIA_VOLTA = 2,
    MOVE_ESQUERDA = 3
};

struct RobotState {
    uint8_t x;
    uint8_t y;
    uint8_t dir;
};

void map_init();

void map_set_wall(uint8_t x, uint8_t y, uint8_t dir, bool present);

bool map_has_wall(uint8_t x, uint8_t y, uint8_t dir);

void map_update_robot_pos(uint8_t x, uint8_t y, uint8_t dir);

void map_print();

// Executa o algoritmo Flood Fill a partir do centro
void map_flood_fill();

// Retorna a melhor ação de movimento baseada nas distâncias
MoveAction map_decide_move();

// Função de mock para validar CT-14 e CT-45 via Monitor Serial
void testar_flood_fill();

#endif
