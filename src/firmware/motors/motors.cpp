#include "motors.h"
#include "../pins.h"

static void motor_set(uint8_t chFwd, uint8_t chBwd, int velocidade_percentual) {

    velocidade_percentual = constrain(velocidade_percentual, -100, 100);

    int pwm = converter_percentual_para_pwm(abs(velocidade_percentual));

    if (velocidade_percentual > 0) {
        ledcWrite(chFwd,  pwm);
        ledcWrite(chBwd,  0);
    } else if (velocidade_percentual < 0) {
        ledcWrite(chFwd,  0);
        ledcWrite(chBwd,  pwm);
    } else {
        ledcWrite(chFwd, 0);
        ledcWrite(chBwd, 0);
    }
}

void motors_init() {
    ledcSetup(CH_MOT1_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOT1_IN1, CH_MOT1_IN1);

    ledcSetup(CH_MOT1_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOT1_IN2, CH_MOT1_IN2);

    ledcSetup(CH_MOT2_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOT2_IN1, CH_MOT2_IN1);

    ledcSetup(CH_MOT2_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOT2_IN2, CH_MOT2_IN2);

    motors_stop_all();
}

int converter_percentual_para_pwm(int percentual) {
    // 1. Garante que o valor não passa de 100 nem desce de 0
    percentual = constrain(percentual, 0, 100);
    
    // 2. Converte a escala de 0-100 para 0-255
    return map(percentual, 0, 100, 0, 255);
}

void motor_esquerdo_set(int velocidade) {
    // Invertido via software: os fios da placa ficaram com polaridade trocada
    motor_set(CH_MOT1_IN1, CH_MOT1_IN2, -velocidade);
}

void motor_direito_set(int velocidade) {
    // Invertido via software: os fios da placa ficaram com polaridade trocada
    motor_set(CH_MOT2_IN1, CH_MOT2_IN2, -velocidade);
}

void motors_stop_all() {
    // Brake ativo: ambos os pinos em HIGH → DRV8833 trava o motor eletricamente
    // Necessário para parada precisa nas células do labirinto
    ledcWrite(CH_MOT1_IN1, 255);
    ledcWrite(CH_MOT1_IN2, 255);
    ledcWrite(CH_MOT2_IN1, 255);
    ledcWrite(CH_MOT2_IN2, 255);
}

void motors_coast_all() {
    // Coast: ambos os pinos em LOW → motor desacelera por inércia
    // Usar em desacelerações graduais e transições suaves
    ledcWrite(CH_MOT1_IN1, 0);
    ledcWrite(CH_MOT1_IN2, 0);
    ledcWrite(CH_MOT2_IN1, 0);
    ledcWrite(CH_MOT2_IN2, 0);
}