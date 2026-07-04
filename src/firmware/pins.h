#ifndef PINS_H
#define PINS_H

// ── I2C ──────────────────────────────────────────────
#define PIN_I2C_SDA     21 // SDA: Pino de dados para MPU-6500 e VL53L0X (Barramento compartilhado)
#define PIN_I2C_SCL     22 // SCL: Pino de clock para MPU-6500 e VL53L0X (Barramento compartilhado)

// ── Motores (DRV8833) ─────────────────────────────────
// Fonte: caderno (não definido no esquemático)
#define PIN_MOT1_IN1    14 // AIN1: Direção do motor esquerdo (M1)
#define PIN_MOT1_IN2    27 // AIN2: Direção do motor esquerdo (M1)
#define PIN_MOT2_IN1    26 // BIN1: Direção do motor direito (M2)
#define PIN_MOT2_IN2    25 // BIN2: Direção do motor direito (M2)
// #define PIN_MOT_SLEEP   // A definir: Output digital para sleep do driver
// #define PIN_MOT_FAULT   // A definir: Input digital para sinalização de falha do driver

// ── Encoders ──────────────────────────────────────────
// Fonte: esquemático (M2 movido de 34/35 para 18/19)
#define PIN_ENC1_A      32 // M1_ENC_A: Fase A do encoder do motor esquerdo (com pull-up interno)
#define PIN_ENC1_B      33 // M1_ENC_B: Fase B do encoder do motor esquerdo (com pull-up interno)
#define PIN_ENC2_A      18 // M2_ENC_A: Fase A do encoder do motor direito
#define PIN_ENC2_B      19 // M2_ENC_B: Fase B do encoder do motor direito

// ── Sensores ToF (VL53L0X) ────────────────────────────
// Fonte: caderno (não definido no esquemático)
#define PIN_TOF1_XSHUT  4  // XSHUT do ToF #1: Ativação seletiva
#define PIN_TOF2_XSHUT  16 // XSHUT do ToF #2: Ativação seletiva (Rx2)
#define PIN_TOF3_XSHUT  17 // XSHUT do ToF #3: Ativação seletiva (Tx2)

// ── Bateria ───────────────────────────────────────────
#define PIN_BAT_ADC     5  // Divisor resistivo da bateria (GPIO5 = ADC2; cuidado: conflita com Wi-Fi ativo)

#endif // PINS_H
