#include "bateria.h"
#include "../pins.h"

// Fator multiplicador baseado no divisor de tensão:
// R1 = 20k (VBat), R2 = 10k (GND)
// V_pin = V_bat * (R2 / (R1 + R2)) => V_bat = V_pin * ((R1 + R2) / R2) = V_pin * 3.0
static const float FATOR_DIVISOR = 3.0f;

static float media_mv = 0.0f;
static bool primeira_leitura = true;

float ler_tensao_bateria() {
    uint32_t leitura_mv = analogReadMilliVolts(PIN_BAT_ADC);
    
    // Filtro passa-baixa simples (Média móvel exponencial)
    // Reduz drasticamente o ruído sem usar `delay` e sem arrays.
    if (primeira_leitura) {
        media_mv = leitura_mv;
        primeira_leitura = false;
    } else {
        media_mv = (media_mv * 0.9f) + (leitura_mv * 0.1f);
    }
    
    // Converte para Volts e aplica o fator do divisor de tensão
    float tensao_volts = (media_mv / 1000.0f) * FATOR_DIVISOR;
    
    return tensao_volts;
}
