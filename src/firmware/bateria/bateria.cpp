#include "bateria.h"
#include "../pins.h"

// Fator multiplicador baseado no divisor de tensão:
// R1 = 20k (VBat), R2 = 10k (GND)
// V_pin = V_bat * (R2 / (R1 + R2)) => V_bat = V_pin * ((R1 + R2) / R2) = V_pin * 3.0
static const float FATOR_DIVISOR = 3.0f;

float ler_tensao_bateria() {
    uint32_t soma_mv = 0;
    const int num_leituras = 10;
    
    // Fazemos múltiplas leituras para aplicar uma média simples e reduzir o ruído do ADC
    for (int i = 0; i < num_leituras; i++) {
        // analogReadMilliVolts usa a calibração de fábrica do ADC do ESP32,
        // retornando o valor lido no pino já em milivolts.
        soma_mv += analogReadMilliVolts(PIN_BAT_ADC);
        delay(1); // Pequeno atraso para estabilizar a leitura
    }
    
    float media_mv = (float)soma_mv / (float)num_leituras;
    
    // Converte para Volts e aplica o fator do divisor de tensão
    float tensao_volts = (media_mv / 1000.0f) * FATOR_DIVISOR;
    
    return tensao_volts;
}
