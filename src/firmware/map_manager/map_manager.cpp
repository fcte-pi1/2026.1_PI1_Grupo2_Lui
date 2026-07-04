#include "map_manager.h"

static bool knownWalls[MAZE_SIZE][MAZE_SIZE][4];
static RobotState robot = {0, MAZE_SIZE - 1, NORTE};

void map_init() {
    for (uint8_t x = 0; x < MAZE_SIZE; x++) {
        for (uint8_t y = 0; y < MAZE_SIZE; y++) {
            for (uint8_t d = 0; d < 4; d++) {
                knownWalls[x][y][d] = false;
            }
        }
    }

    for (uint8_t i = 0; i < MAZE_SIZE; i++) {
        knownWalls[i][0][NORTE] = true;               
        knownWalls[MAZE_SIZE - 1][i][LESTE] = true;    
        knownWalls[i][MAZE_SIZE - 1][SUL] = true;      
        knownWalls[0][i][OESTE] = true;                
    }

    map_set_wall(0, MAZE_SIZE - 1, LESTE, true);
    map_set_wall(0, MAZE_SIZE - 1, OESTE, true);
    map_set_wall(0, MAZE_SIZE - 1, SUL, true);
}

void map_set_wall(uint8_t x, uint8_t y, uint8_t dir, bool present) {
    if (x >= MAZE_SIZE || y >= MAZE_SIZE || dir > 3) return;

    knownWalls[x][y][dir] = present;

    int8_t nx = x;
    int8_t ny = y;
    
    if (dir == NORTE) ny--;
    else if (dir == SUL) ny++;
    else if (dir == LESTE) nx++;
    else if (dir == OESTE) nx--;

    if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
        uint8_t opposite_dir = (dir + 2) % 4;
        knownWalls[nx][ny][opposite_dir] = present;
    }
}

bool map_has_wall(uint8_t x, uint8_t y, uint8_t dir) {
    if (x >= MAZE_SIZE || y >= MAZE_SIZE || dir > 3) return true;
    return knownWalls[x][y][dir];
}

void map_update_robot_pos(uint8_t x, uint8_t y, uint8_t dir) {
    robot.x = x;
    robot.y = y;
    robot.dir = dir;
}

void map_print() {
    Serial.println("=== MAPA ===");
    for (uint8_t y = 0; y < MAZE_SIZE; y++) {
        for (uint8_t x = 0; x < MAZE_SIZE; x++) {
            Serial.print("+");
            if (knownWalls[x][y][NORTE]) Serial.print("---");
            else Serial.print("   ");
        }
        Serial.println("+");

        for (uint8_t x = 0; x < MAZE_SIZE; x++) {
            if (knownWalls[x][y][OESTE]) Serial.print("|");
            else Serial.print(" ");
            
            if (robot.x == x && robot.y == y) {
                if (robot.dir == NORTE) Serial.print(" ^ ");
                else if (robot.dir == SUL) Serial.print(" v ");
                else if (robot.dir == LESTE) Serial.print(" > ");
                else if (robot.dir == OESTE) Serial.print(" < ");
            } else {
                Serial.print("   ");
            }
        }
        if (knownWalls[MAZE_SIZE - 1][y][LESTE]) Serial.print("|");
        Serial.println();
    }
    
    for (uint8_t x = 0; x < MAZE_SIZE; x++) {
        Serial.print("+");
        if (knownWalls[x][MAZE_SIZE - 1][SUL]) Serial.print("---");
        else Serial.print("   ");
    }
    Serial.println("+");
    Serial.println("============");
}
