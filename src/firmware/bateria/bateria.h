#ifndef BATERIA_H
#define BATERIA_H

#include <Arduino.h>

// Lê a tensão atual da bateria usando o ADC1 e o divisor resistivo.
// Retorna a tensão em Volts (ex: 8.4V).
float ler_tensao_bateria();

#endif // BATERIA_H
