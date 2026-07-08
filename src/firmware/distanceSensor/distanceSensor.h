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

// Se qualquer ToF cair abaixo desse valor, os motores sao travados.
#define LIMITE_EMERGENCIA_TOF_MM 20   // 2 cm

// Histerese para liberar a trava depois que o robo se afastar da parede.
#define LIMITE_REARME_EMERGENCIA_MM 50 // 5 cm

#define EMERGENCIA_TOF_ESQ       0x01
#define EMERGENCIA_TOF_FRENTE    0x02
#define EMERGENCIA_TOF_DIR       0x04

#define NUM_AMOSTRAS               5

// Offset fixo do sensor Esquerdo (TOF1). Somado à leitura bruta
//   (negativo = sensor lê mais longe do que o real).
#define DESVIO_SENSOR_1          -65   // mm


void atualizar_filtro_media();
uint16_t distancia_direita_mm();   // Média filtrada do sensor direito (mm)
uint16_t distancia_esquerda_mm();  // Média filtrada do sensor esquerdo (mm)
uint16_t distancia_frente_mm();    // Media filtrada do sensor frontal (mm)
bool tem_parede_esquerda();
bool tem_parede_frente();
bool tem_parede_direita();
bool verificar_emergencia();
bool emergencia_ativa();
uint8_t emergencia_tof_lados();
void limpar_emergencia_se_seguro();
void testar_sensores_paredes();

// ── Acesso à média filtrada dos sensores (em mm) ─────
// Retorna a distância média atual de cada sensor.
// Usado pelo módulo de centralização para correção lateral.
uint16_t obterDistanciaEsq();
uint16_t obterDistanciaFrente();
uint16_t obterDistanciaDir();

#endif
