#ifndef PINS_H
#define PINS_H

// ── I2C ──────────────────────────────────────────────
#define PIN_I2C_SDA     21 // SDA: Pino de dados para MPU-6500 e VL53L0X (Barramento compartilhado)
#define PIN_I2C_SCL     22 // SCL: Pino de clock para MPU-6500 e VL53L0X (Barramento compartilhado)

// ── Motores (DRV8833) ─────────────────────────────────
#define PIN_MOT1_IN1    18 // AIN1: Direção do motor esquerdo (M1)
#define PIN_MOT1_IN2    19 // AIN2: Direção do motor esquerdo (M1)
#define PIN_MOT2_IN1    25 // BIN1: Direção do motor direito (M2)
#define PIN_MOT2_IN2    26 // BIN2: Direção do motor direito (M2)
// #define PIN_MOT_SLEEP   // A definir: Output digital para sleep do driver
// #define PIN_MOT_FAULT   // A definir: Input digital para sinalização de falha do driver

// ── Encoders ──────────────────────────────────────────
#define PIN_ENC1_A      32 // Fase A do encoder do motor esquerdo (com pull-up interno)
#define PIN_ENC1_B      33 // Fase B do encoder do motor esquerdo (com pull-up interno)
#define PIN_ENC2_A      34 // Fase A do encoder do motor direito (Input-only, requer pull-up externo 10 KΩ)
#define PIN_ENC2_B      35 // Fase B do encoder do motor direito (Input-only, requer pull-up externo 10 KΩ)

// ── Sensores ToF (VL53L0X) ────────────────────────────
#define PIN_TOF1_XSHUT  4  // XSHUT do ToF #1: Ativação seletiva
#define PIN_TOF2_XSHUT  5  // XSHUT do ToF #2: Ativação seletiva (Strapping pin - cuidado no boot)
#define PIN_TOF3_XSHUT  15 // XSHUT do ToF #3: Ativação seletiva (Strapping pin - cuidado no boot)

// ── Bateria ───────────────────────────────────────────
// #define PIN_BAT_ADC 36 // Divisor resistivo da bateria (ADC1_CH0). Resistores: R1 = 20k (VBat), R2 = 10k (GND). Fator = 3.
#define PIN_BAT_ADC     36

#endif // PINS_H
