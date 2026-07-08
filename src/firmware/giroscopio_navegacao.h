#ifndef GIROSCOPIO_NAVEGACAO_H
#define GIROSCOPIO_NAVEGACAO_H

#include <Arduino.h>

/**
 * @brief Inicializa o giroscopio (MPU6500) via I2C, verifica a comunicacao 
 * e executa a calibracao inicial (manter o robo parado).
 */
void giroscopioNav_init();

/**
 * @brief Deve ser chamado no loop (ou via interrupcao) para atualizar
 * a integracao do Yaw em background.
 */
void giroscopioNav_update();

/**
 * @brief Gira o robo no proprio eixo pelo angulo especificado, usando o
 * giroscopio em malha fechada e controle proporcional de velocidade.
 * @param graus Angulo em graus. Positivo = esquerda, Negativo = direita.
 */
void giroscopioNav_girarGraus(float graus);

/**
 * @brief Retorna o angulo Z (yaw) atual (em graus) desde a ultima vez que foi zerado.
 */
float giroscopioNav_getYaw();

/**
 * @brief Zera o acumulador de yaw.
 */
void giroscopioNav_resetYaw();

#endif
