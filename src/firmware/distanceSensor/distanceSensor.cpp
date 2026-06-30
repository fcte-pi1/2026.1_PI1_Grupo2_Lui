#include "distanceSensor.h"
#include <Wire.h>
#include <VL53L0X.h>
#include "pins.h"
#include "../motors/motors.h"

// Criando os objetos para cada sensor
VL53L0X sensorEsq;
VL53L0X sensorFrente;
VL53L0X sensorDir;

// Definindo os novos endereços I2C (7-bit)
#define ENDERECO_TOF_ESQ    0x30
#define ENDERECO_TOF_FRENTE 0x31
#define ENDERECO_TOF_DIR    0x32

void configurarSensoresToF() {

  // Configurar os pinos XSHUT como saída
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
    sensorEsq.setMeasurementTimingBudget(20000); // Reduz o tempo de captura para 20ms
    sensorEsq.startContinuous(20); // Configura para modo de alta velocidade (leitura a cada 20ms)
  }

  // Inicializar Sensor Frontal (TOF2)
  digitalWrite(PIN_TOF2_XSHUT, HIGH);
  delay(10); // Tempo para o sensor ligar
  sensorFrente.setTimeout(500);
  if (!sensorFrente.init()) {
    Serial.println("Falha ao iniciar o Sensor Frontal!");
  } else {
    sensorFrente.setAddress(ENDERECO_TOF_FRENTE);
    sensorFrente.setMeasurementTimingBudget(20000); // Reduz o tempo de captura para 20ms
    sensorFrente.startContinuous(20); // Configura para modo de alta velocidade (leitura a cada 20ms)
  }

  // Inicializar Sensor Direito (TOF3)
  digitalWrite(PIN_TOF3_XSHUT, HIGH);
  delay(10); // Tempo para o sensor ligar
  sensorDir.setTimeout(500);
  if (!sensorDir.init()) {
    Serial.println("Falha ao iniciar o Sensor Direito!");
  } else {
    sensorDir.setAddress(ENDERECO_TOF_DIR);
    sensorDir.setMeasurementTimingBudget(20000); // Reduz o tempo de captura para 20ms
    sensorDir.startContinuous(20); // Configura para modo de alta velocidade (leitura a cada 20ms)
  }
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

  // Lê os valores de distância em milímetros
  uint16_t distEsq = sensorEsq.readRangeContinuousMillimeters();
  uint16_t distFrente = sensorFrente.readRangeContinuousMillimeters();
  uint16_t distDir = sensorDir.readRangeContinuousMillimeters();

  // Verifica se houve Timeout
  bool erro = false;
  if (sensorEsq.timeoutOccurred()) { Serial.print("ERRO_ESQ "); erro = true; }
  if (sensorFrente.timeoutOccurred()) { Serial.print("ERRO_FRENTE "); erro = true; }
  if (sensorDir.timeoutOccurred()) { Serial.print("ERRO_DIR "); erro = true; }

  // Se deu erro quebra a linha para não bagunçar o terminal
  if (erro) {
    Serial.println();
  }
  else {
    // Exibição e tratamento do "Fora de Alcance" (> 8000)
    Serial.print("Esq: ");
    if (distEsq > 8000) Serial.print(">Max"); else Serial.print(distEsq);

    Serial.print(" mm\t|\tFrente: ");
    if (distFrente > 8000) Serial.print(">Max"); else Serial.print(distFrente);

    Serial.print(" mm\t|\tDir: ");
    if (distDir > 8000) Serial.print(">Max"); else Serial.print(distDir);

    Serial.println(" mm");
  }

  // Delay para testes, podemos tirar depois para ter leituras mais rápidas
  delay(50);
}

// Buffers circulares — um por sensor, totalmente independentes
static uint16_t bufferEsq[NUM_AMOSTRAS]    = {0};
static uint16_t bufferFrente[NUM_AMOSTRAS] = {0};
static uint16_t bufferDir[NUM_AMOSTRAS]    = {0};
static int  bufferIndex = 0;
static bool bufferCheio = false;

// Estados anteriores para detecção de transição
static bool estadoParedeEsq    = false;
static bool estadoParedeFrente = false;
static bool estadoParedeDir    = false;

void atualizar_filtro_media() {
    ToFSensorReading leitura = lerTodosSensores();

    // Se houve timeout, valor alto usado (8190)
    uint16_t valEsq = leitura.erroEsq ? 8190 : leitura.distEsq;
    uint16_t valFrente = leitura.erroFrente ? 8190 : leitura.distFrente;
    uint16_t valDir = leitura.erroDir ? 8190 : leitura.distDir;

    // Aplicar correção no sensor Esquerdo (Sensor 1)
    if (valEsq < 8000) {
        int corrigido = (int)valEsq + DESVIO_SENSOR_1;
        valEsq = (corrigido > 0) ? (uint16_t)corrigido : 0;
    }

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
    if (maxIt == 0) return 8190; // Sem leituras

    uint32_t soma = 0;
    for (int i = 0; i < maxIt; i++) {
        soma += buffer[i];
    }
    return (uint16_t)(soma / maxIt);
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

void verificar_emergencia() {
    if (calcularMedia(bufferFrente) < LIMITE_SEGURANCA_FRENTE) {
        motors_stop_all();
        Serial.println("[EMERGÊNCIA] Obstáculo iminente! Freando os motores!");
    }
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
