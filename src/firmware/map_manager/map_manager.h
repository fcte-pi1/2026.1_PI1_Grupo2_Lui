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

#endif
