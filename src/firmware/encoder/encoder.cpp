#include "encoder.h"
#include "../pins.h"

// ── Estado interno (volátil — modificado nas ISRs) ────
static volatile long enc1_count = 0;
static volatile int  enc1_dir   = 0;
static volatile long enc2_count = 0;
static volatile int  enc2_dir   = 0;

// ── ISR — Encoder esquerdo (GPIOs 32/33) ─────────────
// Dispara em qualquer borda da fase A.
// Decodificação quadratura: compara A com B no momento da borda.
//   A == B → frente   (+1)
//   A != B → ré       (-1)
static void IRAM_ATTR isr_enc1_a() {
    bool a = digitalRead(PIN_ENC1_A);
    bool b = digitalRead(PIN_ENC1_B);
    if (a == b) { enc1_count++; enc1_dir =  1; }
    else        { enc1_count--; enc1_dir = -1; }
}

// ── ISR — Encoder direito (GPIOs 34/35) ──────────────
// GPIOs 34/35 são input-only no ESP32: sem pull-up interno.
// Pull-up externo de 10 kΩ obrigatório (previsto no pins.h e esquemático).
static void IRAM_ATTR isr_enc2_a() {
    bool a = digitalRead(PIN_ENC2_A);
    bool b = digitalRead(PIN_ENC2_B);
    if (a == b) { enc2_count++; enc2_dir =  1; }
    else        { enc2_count--; enc2_dir = -1; }
}

// ── Inicialização ─────────────────────────────────────
void encoders_init() {
    // Esquerdo: GPIOs 32/33 suportam pull-up interno
    pinMode(PIN_ENC1_A, INPUT_PULLUP);
    pinMode(PIN_ENC1_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC1_A), isr_enc1_a, CHANGE);

    // Direito: GPIOs 18/19 suportam pull-up interno (atualizado conforme pins.h)
    pinMode(PIN_ENC2_A, INPUT_PULLUP);
    pinMode(PIN_ENC2_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC2_A), isr_enc2_a, CHANGE);
}

// ── Leitura thread-safe (desabilita interrupções) ─────
long encoder_esquerdo_get() {
    noInterrupts();
    long v = enc1_count;
    interrupts();
    return v;
}

long encoder_direito_get() {
    noInterrupts();
    long v = enc2_count;
    interrupts();
    return v;
}

// ── Reset dos contadores ──────────────────────────────
void encoder_esquerdo_reset() {
    noInterrupts();
    enc1_count = 0;
    interrupts();
}

void encoder_direito_reset() {
    noInterrupts();
    enc2_count = 0;
    interrupts();
}

// ── Sentido de rotação ────────────────────────────────
int encoder_esquerdo_sentido() { return enc1_dir; }
int encoder_direito_sentido()  { return enc2_dir; }
