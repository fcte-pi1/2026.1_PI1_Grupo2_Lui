#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include <Arduino.h>

struct ToFSensorReading {
    uint16_t distEsq;
    uint16_t distFrente;
    uint16_t distDir;
    bool erroEsq;
    bool erroFrente;
    bool erroDir;
};

// Inicializa os sensores de Distância ToF
void configurarSensoresToF();

// Lê os valores dos sensores ToF e exibe
void lerExibirSensoresToF();

// Lê todos os sensores e retorna as leituras agrupadas
ToFSensorReading lerTodosSensores();

// Recalibrar no labirinto real com o robô
//   centralizado na célula, lendo com e sem parede.
#define LIMITE_PAREDE           130

// Se cair abaixo desse valor, os motores são parados
#define LIMITE_SEGURANCA_FRENTE  40   // mm

#define NUM_AMOSTRAS               5

// Offset fixo do sensor Esquerdo (TOF1). Somado à leitura bruta
//   (negativo = sensor lê mais longe do que o real).
#define DESVIO_SENSOR_1          -65   // mm


void atualizar_filtro_media();
bool tem_parede_esquerda();
bool tem_parede_frente();
bool tem_parede_direita();
uint16_t obter_distancia_esquerda();
uint16_t obter_distancia_direita();
void verificar_emergencia();
void testar_sensores_paredes();

#endif
