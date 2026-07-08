#include "distanceSensor.h"
#include <Wire.h>
#include <VL53L0X.h>
#include "pins.h"
#include "../motors/motors.h"

// Criando os objetos para cada sensor
VL53L0X sensorEsq;
VL53L0X sensorFrente;
VL53L0X sensorDir;
static bool sensoresConfigurados = false;

// Definindo os novos enderecos I2C (7-bit)
#define ENDERECO_TOF_ESQ    0x30
#define ENDERECO_TOF_FRENTE 0x31
#define ENDERECO_TOF_DIR    0x32

void configurarSensoresToF() {

  // Configurar os pinos XSHUT como saida
  pinMode(PIN_TOF1_XSHUT, OUTPUT);
  pinMode(PIN_TOF2_XSHUT, OUTPUT);
  pinMode(PIN_TOF3_XSHUT, OUTPUT);

  // Desligar todos os sensores (Reset) puxando XSHUT para LOW
  digitalWrite(PIN_TOF1_XSHUT, LOW);
  digitalWrite(PIN_TOF2_XSHUT, LOW);
  digitalWrite(PIN_TOF3_XSHUT, LOW);
  delay(10); // Aguarda os sensores desligarem completamente

  // Inicializar Sensor Esquerdo (TOF1)
  digitalWrite(PIN_TOF1_XSHUT, HIGH);
  delay(10); // Tempo para o sensor ligar
  sensorEsq.setTimeout(500);
  if (!sensorEsq.init()) {
    Serial.println("Falha ao iniciar o Sensor Esquerdo!");
  } else {
    sensorEsq.setAddress(ENDERECO_TOF_ESQ);
    sensorEsq.setMeasurementTimingBudget(20000);
    sensorEsq.startContinuous(20);
  }

  // Inicializar Sensor Frontal (TOF2)
  digitalWrite(PIN_TOF2_XSHUT, HIGH);
  delay(10); // Tempo para o sensor ligar
  sensorFrente.setTimeout(500);
  if (!sensorFrente.init()) {
    Serial.println("Falha ao iniciar o Sensor Frontal!");
  } else {
    sensorFrente.setAddress(ENDERECO_TOF_FRENTE);
    sensorFrente.setMeasurementTimingBudget(20000);
    sensorFrente.startContinuous(20);
  }

  // Inicializar Sensor Direito (TOF3)
  digitalWrite(PIN_TOF3_XSHUT, HIGH);
  delay(10); // Tempo para o sensor ligar
  sensorDir.setTimeout(500);
  if (!sensorDir.init()) {
    Serial.println("Falha ao iniciar o Sensor Direito!");
  } else {
    sensorDir.setAddress(ENDERECO_TOF_DIR);
    sensorDir.setMeasurementTimingBudget(20000);
    sensorDir.startContinuous(20);
  }

  sensoresConfigurados = true;
}

ToFSensorReading lerTodosSensores() {
  ToFSensorReading reading;

  reading.distEsq = sensorEsq.readRangeContinuousMillimeters();
  reading.distFrente = sensorFrente.readRangeContinuousMillimeters();
  reading.distDir = sensorDir.readRangeContinuousMillimeters();

  reading.erroEsq = sensorEsq.timeoutOccurred();
  reading.erroFrente = sensorFrente.timeoutOccurred();
  reading.erroDir = sensorDir.timeoutOccurred();

  return reading;
}

void lerExibirSensoresToF() {

  // Le os valores de distancia em milimetros
  uint16_t distEsq = sensorEsq.readRangeContinuousMillimeters();
  uint16_t distFrente = sensorFrente.readRangeContinuousMillimeters();
  uint16_t distDir = sensorDir.readRangeContinuousMillimeters();

  // Verifica se houve Timeout
  bool erro = false;
  if (sensorEsq.timeoutOccurred()) { Serial.print("ERRO_ESQ "); erro = true; }
  if (sensorFrente.timeoutOccurred()) { Serial.print("ERRO_FRENTE "); erro = true; }
  if (sensorDir.timeoutOccurred()) { Serial.print("ERRO_DIR "); erro = true; }

  if (erro) {
    Serial.println();
  }
  else {
    Serial.print("Esq: ");
    if (distEsq > 8000) Serial.print(">Max"); else Serial.print(distEsq);

    Serial.print(" mm\t|\tFrente: ");
    if (distFrente > 8000) Serial.print(">Max"); else Serial.print(distFrente);

    Serial.print(" mm\t|\tDir: ");
    if (distDir > 8000) Serial.print(">Max"); else Serial.print(distDir);

    Serial.println(" mm");
  }

  delay(50);
}

static uint16_t bufferEsq[NUM_AMOSTRAS]    = {0};
static uint16_t bufferFrente[NUM_AMOSTRAS] = {0};
static uint16_t bufferDir[NUM_AMOSTRAS]    = {0};
static int  bufferIndex = 0;
static bool bufferCheio = false;

static uint16_t ultimaEsq = 8190;
static uint16_t ultimaFrente = 8190;
static uint16_t ultimaDir = 8190;
static bool emergenciaAtiva = false;
static uint8_t emergenciaLados = 0;

// Estados anteriores para deteccao de transicao
static bool estadoParedeEsq    = false;
static bool estadoParedeFrente = false;
static bool estadoParedeDir    = false;

void atualizar_filtro_media() {
    if (!sensoresConfigurados) {
        return;
    }

    ToFSensorReading leitura = lerTodosSensores();

    // Se houve timeout, valor alto usado (8190)
    uint16_t valEsq = leitura.erroEsq ? 8190 : leitura.distEsq;
    uint16_t valFrente = leitura.erroFrente ? 8190 : leitura.distFrente;
    uint16_t valDir = leitura.erroDir ? 8190 : leitura.distDir;

    // Aplicar correcao no sensor Esquerdo (Sensor 1)
    if (valEsq < 8000) {
        int corrigido = (int)valEsq + DESVIO_SENSOR_1;
        valEsq = (corrigido > 0) ? (uint16_t)corrigido : 0;
    }

    ultimaEsq = valEsq;
    ultimaFrente = valFrente;
    ultimaDir = valDir;

    bufferEsq[bufferIndex] = valEsq;
    bufferFrente[bufferIndex] = valFrente;
    bufferDir[bufferIndex] = valDir;

    bufferIndex++;
    if (bufferIndex >= NUM_AMOSTRAS) {
        bufferIndex = 0;
        bufferCheio = true;
    }
}

static uint16_t calcularMedia(uint16_t* buffer) {
    int maxIt = bufferCheio ? NUM_AMOSTRAS : bufferIndex;
    if (maxIt == 0) return 8190;

    uint32_t soma = 0;
    for (int i = 0; i < maxIt; i++) {
        soma += buffer[i];
    }
    return (uint16_t)(soma / maxIt);
}

uint16_t distancia_direita_mm() {
    return calcularMedia(bufferDir);
}

uint16_t distancia_esquerda_mm() {
    return calcularMedia(bufferEsq);
}

uint16_t distancia_frente_mm() {
    return calcularMedia(bufferFrente);
}

bool tem_parede_esquerda() {
    return calcularMedia(bufferEsq) < LIMITE_PAREDE;
}

bool tem_parede_frente() {
    return calcularMedia(bufferFrente) < LIMITE_PAREDE;
}

bool tem_parede_direita() {
    return calcularMedia(bufferDir) < LIMITE_PAREDE;
}

static bool leituraValida(uint16_t valor) {
    return valor < 8000;
}

static uint8_t ladosEmEmergencia(uint16_t limite) {
    uint8_t lados = 0;
    if (leituraValida(ultimaEsq) && ultimaEsq <= limite) lados |= EMERGENCIA_TOF_ESQ;
    if (leituraValida(ultimaFrente) && ultimaFrente <= limite) lados |= EMERGENCIA_TOF_FRENTE;
    if (leituraValida(ultimaDir) && ultimaDir <= limite) lados |= EMERGENCIA_TOF_DIR;
    return lados;
}

bool emergencia_ativa() {
    return emergenciaAtiva;
}

uint8_t emergencia_tof_lados() {
    return emergenciaLados;
}

void limpar_emergencia_se_seguro() {
    if (ladosEmEmergencia(LIMITE_REARME_EMERGENCIA_MM) == 0) {
        emergenciaAtiva = false;
        emergenciaLados = 0;
    }
}

bool verificar_emergencia() {
    uint8_t lados = ladosEmEmergencia(LIMITE_EMERGENCIA_TOF_MM);

    if (lados != 0) {
        bool novaEmergencia = !emergenciaAtiva;
        emergenciaAtiva = true;
        emergenciaLados = lados;
        motors_stop_all();
        if (novaEmergencia) {
            Serial.print("[EMERGENCIA] Parede a 2 cm ou menos. Lados=");
            Serial.println(emergenciaLados, BIN);
        }
        return true;
    }

    if (emergenciaAtiva) {
        limpar_emergencia_se_seguro();
        if (emergenciaAtiva) {
            motors_stop_all();
        }
    }

    return emergenciaAtiva;
}

void testar_sensores_paredes() {
    bool novaEsq = tem_parede_esquerda();
    bool novaFrente = tem_parede_frente();
    bool novaDir = tem_parede_direita();

    if (novaEsq != estadoParedeEsq) {
        estadoParedeEsq = novaEsq;
        Serial.println(novaEsq ? "[MAP MANAGER] Parede Esquerda Detectada!" : "[MAP MANAGER] Parede Esquerda Removida!");
    }

    if (novaFrente != estadoParedeFrente) {
        estadoParedeFrente = novaFrente;
        Serial.println(novaFrente ? "[MAP MANAGER] Parede Frontal Detectada!" : "[MAP MANAGER] Parede Frontal Removida!");
    }

    if (novaDir != estadoParedeDir) {
        estadoParedeDir = novaDir;
        Serial.println(novaDir ? "[MAP MANAGER] Parede Direita Detectada!" : "[MAP MANAGER] Parede Direita Removida!");
    }
}

// ── Acesso à média filtrada dos sensores ──────────────
uint16_t obterDistanciaEsq() {
    return calcularMedia(bufferEsq);
}

uint16_t obterDistanciaFrente() {
    return calcularMedia(bufferFrente);
}

uint16_t obterDistanciaDir() {
    return calcularMedia(bufferDir);
}

