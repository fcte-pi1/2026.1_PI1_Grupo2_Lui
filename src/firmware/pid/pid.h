#ifndef PID_H
#define PID_H

#include <Arduino.h>

struct PidConfig {
    float Kp;
    float Ki;
    float Kd;
    float erro_anterior;
    float integral;
};

// Retorna o erro atual do robô em relação ao centro do corredor.
// Erro Positivo = muito para a esquerda
// Erro Negativo = muito para a direita
float obter_erro_centralizacao();

// Calcula o ajuste PID baseado no erro e no tempo passado
float calcular_pid(float erro, float dt);

// Reseta o estado interno do PID
void resetar_pid();

#endif
