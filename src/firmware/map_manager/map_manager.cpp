#include "map_manager.h"

static bool knownWalls[MAZE_SIZE][MAZE_SIZE][4];
static uint8_t distances[MAZE_SIZE][MAZE_SIZE];
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
    
    map_flood_fill();
}

void map_set_wall(uint8_t x, uint8_t y, uint8_t dir, bool present) {
    if (x >= MAZE_SIZE || y >= MAZE_SIZE || dir > 3) return;

    bool mudou = (knownWalls[x][y][dir] != present);
    knownWalls[x][y][dir] = present;

    int8_t nx = x;
    int8_t ny = y;
    
    if (dir == NORTE) ny--;
    else if (dir == SUL) ny++;
    else if (dir == LESTE) nx++;
    else if (dir == OESTE) nx--;

    if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
        uint8_t opposite_dir = (dir + 2) % 4;
        if (knownWalls[nx][ny][opposite_dir] != present) mudou = true;
        knownWalls[nx][ny][opposite_dir] = present;
    }

    if (mudou) {
        map_flood_fill();
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

struct Cell { uint8_t x, y; };

void map_flood_fill() {
    for (uint8_t x = 0; x < MAZE_SIZE; x++) {
        for (uint8_t y = 0; y < MAZE_SIZE; y++) {
            distances[x][y] = 255;
        }
    }

    Cell queue[256];
    uint16_t head = 0, tail = 0;

    // Define os centros do labirinto
    uint8_t g1 = MAZE_SIZE / 2 - 1;
    uint8_t g2 = MAZE_SIZE / 2;
    
    distances[g1][g1] = 0; queue[tail++] = {g1, g1};
    distances[g1][g2] = 0; queue[tail++] = {g1, g2};
    distances[g2][g1] = 0; queue[tail++] = {g2, g1};
    distances[g2][g2] = 0; queue[tail++] = {g2, g2};

    int8_t dx[] = {0, 1, 0, -1};
    int8_t dy[] = {-1, 0, 1, 0};

    while (head < tail) {
        Cell c = queue[head++];
        
        for (uint8_t d = 0; d < 4; d++) {
            if (!knownWalls[c.x][c.y][d]) {
                int8_t nx = c.x + dx[d];
                int8_t ny = c.y + dy[d];
                
                if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
                    if (distances[nx][ny] == 255) {
                        distances[nx][ny] = distances[c.x][c.y] + 1;
                        queue[tail++] = {(uint8_t)nx, (uint8_t)ny};
                    }
                }
            }
        }
    }
}

MoveAction map_decide_move() {
    uint8_t best_dir = robot.dir;
    uint8_t min_dist = 255;
    
    int8_t dx[] = {0, 1, 0, -1};
    int8_t dy[] = {-1, 0, 1, 0};

    // Mesma ordem de checagem do useMazeSimulator.js e prioridade (<=)
    uint8_t checkOrder[] = {
        (uint8_t)((robot.dir + 3) % 4), // Esquerda
        (uint8_t)((robot.dir + 2) % 4), // Trás
        (uint8_t)((robot.dir + 1) % 4), // Direita
        robot.dir                       // Frente
    };

    for (int i = 0; i < 4; i++) {
        uint8_t d = checkOrder[i];
        if (!knownWalls[robot.x][robot.y][d]) {
            int8_t nx = robot.x + dx[d];
            int8_t ny = robot.y + dy[d];
            if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
                if (distances[nx][ny] <= min_dist) {
                    min_dist = distances[nx][ny];
                    best_dir = d;
                }
            }
        }
    }

    if (best_dir == robot.dir) return MOVE_FRENTE;
    if (best_dir == (robot.dir + 1) % 4) return MOVE_DIREITA;
    if (best_dir == (robot.dir + 3) % 4) return MOVE_ESQUERDA;
    return MOVE_MEIA_VOLTA;
}

void testar_flood_fill() {
    Serial.println("\n=== INICIO TESTE FLOOD FILL (CT-14 / CT-45) ===");
    
    // Força tamanho simulado (funciona melhor se MAZE_SIZE = 4 no .h, mas roda genérico)
    map_init(); 
    
    // Inserindo paredes de mentira para forçar o robô a contornar (ex: num 4x4)
    // Isso forçará um caminho específico e ativará o recálculo do flood fill.
    if (MAZE_SIZE == 4) {
        map_set_wall(0, 2, LESTE, true);
        map_set_wall(1, 2, LESTE, true);
        map_set_wall(2, 2, NORTE, true);
    } else {
        // Labirinto padrão sem paredes no meio vai fazer ele ir reto pra frente e virar
    }

    int passos = 0;
    while(distances[robot.x][robot.y] > 0 && passos < 30) {
        MoveAction move = map_decide_move();
        
        Serial.print("Passo ");
        Serial.print(passos);
        Serial.print(" - Pos: (");
        Serial.print(robot.x); Serial.print(","); Serial.print(robot.y);
        Serial.print(") Dir: "); Serial.print(robot.dir);
        Serial.print(" -> Acao: ");
        
        if (move == MOVE_FRENTE) Serial.println("FRENTE");
        else if (move == MOVE_DIREITA) Serial.println("DIREITA");
        else if (move == MOVE_ESQUERDA) Serial.println("ESQUERDA");
        else if (move == MOVE_MEIA_VOLTA) Serial.println("MEIA_VOLTA");

        // Atualiza a direcao
        if (move == MOVE_DIREITA) robot.dir = (robot.dir + 1) % 4;
        else if (move == MOVE_ESQUERDA) robot.dir = (robot.dir + 3) % 4;
        else if (move == MOVE_MEIA_VOLTA) robot.dir = (robot.dir + 2) % 4;
        
        // Anda
        int8_t dx[] = {0, 1, 0, -1};
        int8_t dy[] = {-1, 0, 1, 0};
        robot.x += dx[robot.dir];
        robot.y += dy[robot.dir];
        
        passos++;
    }
    
    if (distances[robot.x][robot.y] == 0) {
        Serial.println("=== TESTE CONCLUIDO COM SUCESSO (CENTRO ENCONTRADO) ===");
    } else {
        Serial.println("=== TESTE FALHOU (PRESO OU LIMITADO) ===");
    }
}
