#include "pid.h"
#include "../distanceSensor/distanceSensor.h"
#include "../mpu/mpu.h"

// Configuração inicial dos ganhos PID
static PidConfig config = {
    .Kp = 1.0f,
    .Ki = 0.0f,
    .Kd = 0.0f,
    .erro_anterior = 0.0f,
    .integral = 0.0f
};

#define ALVO_CENTRO 90.0f

float obter_erro_centralizacao() {
    bool paredeEsq = tem_parede_esquerda();
    bool paredeDir = tem_parede_direita();

    float erro = 0.0f;

    if (paredeEsq && paredeDir) {
        // Cenário 1: Paredes dos dois lados.
        float distEsq = (float)obter_distancia_esquerda();
        float distDir = (float)obter_distancia_direita();
        // Erro positivo = robô está colado na esquerda (precisa ir para a direita).
        // Erro negativo = robô está colado na direita (precisa ir para a esquerda).
        erro = distDir - distEsq;
    }
    else if (paredeEsq && !paredeDir) {
        // Cenário 2: Parede apenas na esquerda.
        float distEsq = (float)obter_distancia_esquerda();
        erro = ALVO_CENTRO - distEsq;
    }
    else if (!paredeEsq && paredeDir) {
        // Cenário 3: Parede apenas na direita.
        float distDir = (float)obter_distancia_direita();
        erro = distDir - ALVO_CENTRO;
    }
    else {
        // Cenário 4: Nenhuma parede (corredor aberto/cruzamento).
        float yaw = mpu_get_yaw();
        erro = yaw * 5.0f; // Fator de conversão empírico (graus para "mm de erro")
    }

    return erro;
}

float calcular_pid(float erro, float dt) {
    if (dt <= 0.001f) return 0.0f; // Previne divisão por zero

    config.integral += erro * dt;
    float derivativo = (erro - config.erro_anterior) / dt;

    float saida = (config.Kp * erro) + (config.Ki * config.integral) + (config.Kd * derivativo);

    config.erro_anterior = erro;

    return saida;
}

void resetar_pid() {
    config.integral = 0.0f;
    config.erro_anterior = 0.0f;
}
