// =====================================================================
//  FIRMWARE CENTRALIZADO — MICROMOUSE (2026_1_PI1_Grupo02_Diogo)
//  Gerado em 2026-07-07 mesclando TODA a lógica de src/firmware:
//
//   1. Pinout (referência)          lib/pinout.h
//   2. Rato                         lib/utils/rato/
//   3. Labirinto                    lib/utils/mapa/
//   4. Correção de parede           lib/output/motor/correcao_parede.*
//   5. Sensores IR (VL53L0X/VL6180X) lib/input/infravermelho/   [ATIVO]
//   6. Sensores ultrassônicos       lib/input/ultrassonico/     [#ifdef]
//   7. Motores / PWM / PID          lib/output/motor/motor.*
//   8. Energia (INA219)             lib/input/energia/
//   9. Conexões WiFi/MQTT           lib/utils/conexao/
//  10. DFS                          lib/utils/dfs/
//  11. Telemetria                   lib/utils/telemetria/
//  12. Setup/Loop                   src/main.cpp
//
//  COMO USAR: compile este arquivo sozinho como src/main.cpp
//  (mesmo platformio.ini / lib_deps). Não compile junto com a lib/.
//
//  Ajustes mínimos feitos na mesclagem (comportamento inalterado):
//   - `_dirs`/`_indiceDirecao` do motor renomeados (`_dirsMotor`) para não
//     colidir com `_dirs` do DFS (ambos eram static em arquivos separados);
//   - constantes de correção de parede duplicadas em motor.cpp removidas
//     (já definidas na seção 4);
//   - módulo ultrassônico preservado sob USAR_SENSORES_ULTRASSONICOS, pois
//     define as mesmas funções dos sensores IR (o main usa a versão IR).
//
//  MUDANÇA FUNCIONAL (2026-07-07): a seção 4 (correção de parede) foi
//  substituída pela centralização proporcional contínua adaptada do
//  Grupo1_Lui (branch feature/mpu6050_gyro). Além de contínua, ela corrige
//  o SENTIDO da correção: o original acelerava a roda do lado aberto e
//  freava a do lado da parede (pwmEsq -= / pwmDir += com parede à esquerda),
//  o que vira o robô PARA a parede próxima. Se na prática o robô do grupo
//  centralizava certo com o código antigo, os rótulos esq/dir dos sensores
//  estão trocados no hardware — nesse caso, inverta o sinal de KP_PAREDE_PWM.
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <VL53L0X.h>
#include <VL6180X.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Descomente para usar HC-SR04 no lugar dos sensores IR (exige adaptar o
// setup(), pois inicializaSensores() passa a receber 6 pinos TRIG/ECHO):
// #define USAR_SENSORES_ULTRASSONICOS

// =====================================================================
//  1. PINOUT — lib/pinout.h (referência; o main define seus próprios pinos)
// =====================================================================
#if 0 // não usado pelo main.cpp original — mantido apenas como documentação
const int LED_PIN = 2;

// Sensores Ultrassônicos
const int TRIG_FRONT = 4;   const int ECHO_FRONT = 16;
const int TRIG_LEFT = 17;   const int ECHO_LEFT = 5;
const int TRIG_RIGHT = 18;  const int ECHO_RIGHT = 19;

// Motores (Ponte H)
const int MOTOR_LEFT_IN1 = 25;  const int MOTOR_LEFT_IN2 = 26;
const int MOTOR_RIGHT_IN1 = 27; const int MOTOR_RIGHT_IN2 = 14;

// Encoders
const int ENCODER_LEFT_A = 34;  const int ENCODER_LEFT_B = 35;
const int ENCODER_RIGHT_A = 32; const int ENCODER_RIGHT_B = 33;
#endif

// =====================================================================
//  2. RATO — lib/utils/rato/rato.h + rato.cpp
// =====================================================================
typedef struct Rato
{
    int x, y;     // Posição no labirinto
    char direcao; // 'N', 'S', 'L', 'O'

    // Leituras dos sensores (cm)
    float distancia_frente;
    float distancia_esquerda;
    float distancia_direita;

    // Controle dos motores
    int pwm_motor_esquerdo;
    int pwm_motor_direito;

    // Contagem dos encoders
    long encoder_esquerdo;
    long encoder_direito;

    // Energia
    float corrente;
    float tensao;
} Rato;

void inicializaRato(Rato *rato)
{
    rato->x = 15; // começa no meio da matriz 31x31
    rato->y = 15;
    rato->direcao = 'N';

    rato->distancia_frente = 0.0f;
    rato->distancia_esquerda = 0.0f;
    rato->distancia_direita = 0.0f;

    rato->pwm_motor_esquerdo = 0;
    rato->pwm_motor_direito = 0;

    rato->encoder_esquerdo = 0;
    rato->encoder_direito = 0;

    rato->tensao = 0.0f;
    rato->corrente = 0.0f;
}

// =====================================================================
//  3. LABIRINTO — lib/utils/mapa/labirinto.h + labirinto.cpp
// =====================================================================
#define LAB_TAM 31
#define DIST_INFINITA 32767 // Distancia maxima é 31*31

typedef struct Celula
{
    short distancia;               // valor do flood fill (max 960 para 31×31)
    bool norte, sul, leste, oeste; // paredes conhecidas
    bool explorado;                // robô já visitou
    bool visitado;                 // visitou para o DFS
} Celula;

typedef struct Labirinto
{
    Celula celula[LAB_TAM][LAB_TAM];
} Labirinto;

void inicializaLabirinto(Labirinto *lab)
{
    for (int x = 0; x < LAB_TAM; x++)
    {
        for (int y = 0; y < LAB_TAM; y++)
        {
            lab->celula[x][y].distancia = DIST_INFINITA;
            lab->celula[x][y].norte = false;
            lab->celula[x][y].sul = false;
            lab->celula[x][y].leste = false;
            lab->celula[x][y].oeste = false;
            lab->celula[x][y].explorado = false;
            lab->celula[x][y].visitado = false;
        }
    }

    // Bordas = paredes
    for (int i = 0; i < LAB_TAM; i++)
    {
        lab->celula[i][0].sul = true;
        lab->celula[i][LAB_TAM - 1].norte = true;
        lab->celula[0][i].oeste = true;
        lab->celula[LAB_TAM - 1][i].leste = true;
    }
}

// marcar parede encontrada (a parte de "visitado" e "explorado" tem que ser
// feita dentro do algoritmo de movimento)
void registrarParede(Labirinto *lab, int x, int y, char direcao)
{
    switch (direcao)
    {
    case 'N':
        lab->celula[x][y].norte = true;
        if (y + 1 < LAB_TAM)
            lab->celula[x][y + 1].sul = true;
        break;
    case 'S':
        lab->celula[x][y].sul = true;
        if (y - 1 >= 0)
            lab->celula[x][y - 1].norte = true;
        break;
    case 'L':
        lab->celula[x][y].leste = true;
        if (x + 1 < LAB_TAM)
            lab->celula[x + 1][y].oeste = true;
        break;
    case 'O':
        lab->celula[x][y].oeste = true;
        if (x - 1 >= 0)
            lab->celula[x - 1][y].leste = true;
        break;
    }
}

// =====================================================================
//  4. CORREÇÃO DE PAREDE — centralização proporcional contínua
//     (substitui o gatilho 3cm/±12PWM original por lógica adaptada do
//      wall following do Grupo1_Lui: erro proporcional, três casos —
//      duas paredes / uma parede / nenhuma — e saturação da correção.
//      Mesma assinatura: continua sendo chamada por _corrigirCentralizacao.)
// =====================================================================
static const float PAREDE_VISIVEL_CM = 15.0f;  // parede "conta" se < 15 cm (Lui: 150 mm)
static const float CLEARANCE_IDEAL_CM = 3.25f; // folga ideal p/ célula 18 cm e chassi ~11,5 cm (Lui: 32,5 mm)
static const float KP_PAREDE_PWM = 4.0f;       // PWM por cm de erro (Lui: 0.2 em mm->cm/s)
static const int CORRECAO_MAX_PWM = 20;        // saturação da correção (Lui: ±8 cm/s)
static const int CORRECAO_MAX_CURVA_PWM = 10;  // em curva, metade (substitui o AJUSTE_CURVA antigo)

void aplicarCorrecaoParede(float distEsq, float distDir, int &pwmEsq, int &pwmDir, bool emCurva)
{
    float erroCm = 0.0f;
    bool temParede = false;

    bool esqVisivel = distEsq < PAREDE_VISIVEL_CM;
    bool dirVisivel = distDir < PAREDE_VISIVEL_CM;

    if (esqVisivel && dirVisivel)
    {
        // Duas paredes: fusão — centraliza pela diferença direta
        erroCm = distEsq - distDir;
        temParede = true;
    }
    else if (esqVisivel)
    {
        // Só parede à esquerda: repulsão unilateral apenas se abaixo da folga ideal
        if (distEsq < CLEARANCE_IDEAL_CM)
        {
            erroCm = (distEsq - CLEARANCE_IDEAL_CM) * 2.0f;
            temParede = true;
        }
    }
    else if (dirVisivel)
    {
        // Só parede à direita: repulsão unilateral
        if (distDir < CLEARANCE_IDEAL_CM)
        {
            erroCm = (CLEARANCE_IDEAL_CM - distDir) * 2.0f;
            temParede = true;
        }
    }
    // Nenhuma parede visível: segue apenas no PID de encoders

    if (!temParede)
        return;

    float correcao = erroCm * KP_PAREDE_PWM;
    int maxCorrecao = emCurva ? CORRECAO_MAX_CURVA_PWM : CORRECAO_MAX_PWM;
    if (correcao > maxCorrecao)
        correcao = (float)maxCorrecao;
    if (correcao < -maxCorrecao)
        correcao = (float)-maxCorrecao;

    // Convenção (a mesma do Lui): erro < 0 = mais perto da esquerda ->
    // correcao < 0 -> esquerda acelera, direita desacelera -> vira p/ DIREITA,
    // afastando-se da parede próxima. (O código original fazia o oposto —
    // corrigia PARA a parede mais próxima; ver nota no cabeçalho do arquivo.)
    pwmEsq -= (int)correcao;
    pwmDir += (int)correcao;
}

// =====================================================================
//  SENSORES — constantes comuns às duas implementações
// =====================================================================
#define DISTANCIA_LIVRE_CM 400.0f // quando não ve nada
#define DISTANCIA_PAREDE_CM 12.0f // < 12 = parede

#ifndef USAR_SENSORES_ULTRASSONICOS
// =====================================================================
//  5. SENSORES IR (VL53L0X + VL6180X) — lib/input/infravermelho/  [ATIVO]
// =====================================================================
#define ENDERECO_VL53L0X_ESQ 0x2A
#define ENDERECO_VL53L0X_DIR 0x31
#define ENDERECO_VL6180X_FRENTE 0x30

#define I2C_SDA 21
#define I2C_SCL 22
#define INTERVALO_LEITURA_MS 50

static uint8_t _xshutF, _xshutE, _xshutD;
static VL53L0X _vlEsquerdo;
static VL53L0X _vlDireito;
static VL6180X _vlFrente;

static float _distF = DISTANCIA_LIVRE_CM;
static float _distE = DISTANCIA_LIVRE_CM;
static float _distD = DISTANCIA_LIVRE_CM;

void inicializaSensores(uint8_t xshutFrente, uint8_t xshutEsquerda, uint8_t xshutDireita)
{
    _xshutF = xshutFrente;
    _xshutE = xshutEsquerda;
    _xshutD = xshutDireita;

    pinMode(_xshutF, OUTPUT);
    pinMode(_xshutE, OUTPUT);
    pinMode(_xshutD, OUTPUT);

    digitalWrite(_xshutE, LOW);
    digitalWrite(_xshutD, LOW);
    digitalWrite(_xshutF, LOW);
    delay(10);

    Wire.begin(I2C_SDA, I2C_SCL);

    digitalWrite(_xshutE, HIGH);
    delay(10);
    if (!_vlEsquerdo.init())
    {
        Serial.println("[ERRO] VL53L0X esquerdo nao detectado");
    }
    _vlEsquerdo.setAddress(ENDERECO_VL53L0X_ESQ);

    digitalWrite(_xshutD, HIGH);
    delay(10);
    if (!_vlDireito.init())
    {
        Serial.println("[ERRO] VL53L0X direito nao detectado");
    }
    _vlDireito.setAddress(ENDERECO_VL53L0X_DIR);

    digitalWrite(_xshutF, HIGH);
    delay(10);
    _vlFrente.init();
    _vlFrente.setAddress(ENDERECO_VL6180X_FRENTE);

    Serial.println("[SENSORES_IR] VL53L0X esq=0x2A dir=0x31 VL6180X frente=0x30");
}

void atualizaSensores()
{
    static unsigned long ultimaLeitura = 0;
    unsigned long agora = millis();
    if (agora - ultimaLeitura < INTERVALO_LEITURA_MS)
        return;
    ultimaLeitura = agora;

    uint8_t mmF = _vlFrente.readRangeSingle();
    _distF = _vlFrente.timeoutOccurred() ? DISTANCIA_LIVRE_CM : mmF / 10.0f;

    uint16_t mmE = _vlEsquerdo.readRangeSingleMillimeters();
    _distE = _vlEsquerdo.timeoutOccurred() ? DISTANCIA_LIVRE_CM : mmE / 10.0f;

    uint16_t mmD = _vlDireito.readRangeSingleMillimeters();
    _distD = _vlDireito.timeoutOccurred() ? DISTANCIA_LIVRE_CM : mmD / 10.0f;
}

void lerDistancias(Rato *rato)
{
    noInterrupts();
    rato->distancia_frente = _distF;
    rato->distancia_esquerda = _distE;
    rato->distancia_direita = _distD;
    interrupts();
}

float getDistanciaFrente() { return _distF; }
float getDistanciaEsquerda() { return _distE; }
float getDistanciaDireita() { return _distD; }

bool temParedeFrente() { return _distF < DISTANCIA_PAREDE_CM; }
bool temParedeEsquerda() { return _distE < DISTANCIA_PAREDE_CM; }
bool temParedeDireita() { return _distD < DISTANCIA_PAREDE_CM; }

#else
// =====================================================================
//  6. SENSORES ULTRASSÔNICOS (HC-SR04) — lib/input/ultrassonico/
//     Mesma interface pública da seção 5 (por isso mutuamente exclusivos).
// =====================================================================
#define SENSOR_TIMEOUT_US 12000 // 12 ms ~ 200 cm máximo (alterar)
#define INTERVALO_TRIGGER_MS 20 // ms entre triggers (3 sensores × 20ms = 60ms/ciclo) (alterar)

// -- Pinos (preenchidos em inicializaSensores) --------------------------------
static uint8_t _trigF, _echoF;
static uint8_t _trigE, _echoE;
static uint8_t _trigD, _echoD;

// -- Estado de cada sensor ----------------------------------------------------
// Volatile: escritas na ISR, leituras no loop principal
volatile unsigned long _inicioF = 0, _inicioE = 0, _inicioD = 0;
volatile bool _medindoF = false, _medindoE = false, _medindoD = false;
volatile float _distF = DISTANCIA_LIVRE_CM;
volatile float _distE = DISTANCIA_LIVRE_CM;
volatile float _distD = DISTANCIA_LIVRE_CM;

// -- ISRs (rodam na RISING e FALLING do pino ECHO) ----------------------------
// RISING  → grava o instante de início do pulso
// FALLING → calcula duração e converte para cm
//   distância (cm) = duração (microsegundos) × velocidade_do_som / 2
//                  = duração × 0.0343 / 2 = duração × 0.01715

void IRAM_ATTR _isrFrente()
{
    if (digitalRead(_echoF))
    {
        _inicioF = micros();
        _medindoF = true;
    }
    else if (_medindoF)
    {
        unsigned long dur = micros() - _inicioF;
        _distF = (dur > 0 && dur < SENSOR_TIMEOUT_US)
                     ? dur * 0.01715f
                     : DISTANCIA_LIVRE_CM;
        _medindoF = false;
    }
}

void IRAM_ATTR _isrEsquerda()
{
    if (digitalRead(_echoE))
    {
        _inicioE = micros();
        _medindoE = true;
    }
    else if (_medindoE)
    {
        unsigned long dur = micros() - _inicioE;
        _distE = (dur > 0 && dur < SENSOR_TIMEOUT_US)
                     ? dur * 0.01715f
                     : DISTANCIA_LIVRE_CM;
        _medindoE = false;
    }
}

void IRAM_ATTR _isrDireita()
{
    if (digitalRead(_echoD))
    {
        _inicioD = micros();
        _medindoD = true;
    }
    else if (_medindoD)
    {
        unsigned long dur = micros() - _inicioD;
        _distD = (dur > 0 && dur < SENSOR_TIMEOUT_US)
                     ? dur * 0.01715f
                     : DISTANCIA_LIVRE_CM;
        _medindoD = false;
    }
}

// -- Pulso de trigger -------------------
static void _dispararTrigger(uint8_t trigPin)
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
}

// -- Inicialização -------------------------------------------------------------
void inicializaSensores(uint8_t trigFrente, uint8_t echoFrente,
                        uint8_t trigEsquerda, uint8_t echoEsquerda,
                        uint8_t trigDireita, uint8_t echoDireita)
{
    _trigF = trigFrente;
    _echoF = echoFrente;
    _trigE = trigEsquerda;
    _echoE = echoEsquerda;
    _trigD = trigDireita;
    _echoD = echoDireita;

    pinMode(_trigF, OUTPUT);
    digitalWrite(_trigF, LOW);
    pinMode(_trigE, OUTPUT);
    digitalWrite(_trigE, LOW);
    pinMode(_trigD, OUTPUT);
    digitalWrite(_trigD, LOW);

    pinMode(_echoF, INPUT);
    pinMode(_echoE, INPUT);
    pinMode(_echoD, INPUT);

    // CHANGE dispara na borda de subida (RISING) e descida (FALLING)
    attachInterrupt(digitalPinToInterrupt(_echoF), _isrFrente, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_echoE), _isrEsquerda, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_echoD), _isrDireita, CHANGE);
}

// -- Loop não-bloqueante -------------------------------------------------------
// Dispara um sensor a cada INTERVALO_TRIGGER_MS em rodízio:
//   t=0ms → frente | t=20ms → esquerda | t=40ms → direita | t=60ms → frente ...
// Cada sensor é atualizado a ~16 Hz (alterar no teste).
void atualizaSensores()
{
    static unsigned long ultimoTrigger = 0;
    static uint8_t vez = 0; // 0 = frente | 1 = esquerda | 2 = direita

    unsigned long agora = millis();
    if (agora - ultimoTrigger < INTERVALO_TRIGGER_MS)
        return;
    ultimoTrigger = agora;

    switch (vez)
    {
    case 0:
        _dispararTrigger(_trigF);
        break;
    case 1:
        _dispararTrigger(_trigE);
        break;
    case 2:
        _dispararTrigger(_trigD);
        break;
    }
    vez = (vez + 1) % 3;
}

// -- Leituras ------------------------------------------------------------------
void lerDistancias(Rato *rato)
{
    // desativa interrupções para evitar leitura parcial
    noInterrupts();
    rato->distancia_frente = _distF;
    rato->distancia_esquerda = _distE;
    rato->distancia_direita = _distD;
    interrupts();
}

float getDistanciaFrente() { return _distF; }
float getDistanciaEsquerda() { return _distE; }
float getDistanciaDireita() { return _distD; }

bool temParedeFrente() { return _distF < DISTANCIA_PAREDE_CM; }
bool temParedeEsquerda() { return _distE < DISTANCIA_PAREDE_CM; }
bool temParedeDireita() { return _distD < DISTANCIA_PAREDE_CM; }
#endif // USAR_SENSORES_ULTRASSONICOS

// =====================================================================
//  7. MOTORES / PWM / ODOMETRIA / PID — lib/output/motor/motor.h + .cpp
// =====================================================================
// -- Calibração (tem que alterar) -----------------------------------
#define PULSOS_POR_CELULA 200 // pulsos de encoder para avançar uma célula
#define PULSOS_GIRO_90 95     // pulsos de encoder para girar 90°

// -- Pinos e encoders (registrados em inicializaMotores) ----------------------
static uint8_t _in1L, _in2L;
static uint8_t _in1R, _in2R;
static volatile long *_encEsq;
static volatile long *_encDir;

void inicializaMotores(uint8_t in1L, uint8_t in2L,
                       uint8_t in1R, uint8_t in2R,
                       volatile long *encEsq, volatile long *encDir)
{
    _in1L = in1L;
    _in2L = in2L;
    _in1R = in1R;
    _in2R = in2R;
    _encEsq = encEsq;
    _encDir = encDir;

    pinMode(_in1L, OUTPUT);
    pinMode(_in2L, OUTPUT);
    pinMode(_in1R, OUTPUT);
    pinMode(_in2R, OUTPUT);

    // motors parados no cmc
    digitalWrite(_in1L, LOW);
    digitalWrite(_in2L, LOW);
    digitalWrite(_in1R, LOW);
    digitalWrite(_in2R, LOW);
}

// -- Funções de motor (auxiliar) -----------------------------------------------
static void _stopMotors()
{
    digitalWrite(_in1L, LOW);
    digitalWrite(_in2L, LOW);
    digitalWrite(_in1R, LOW);
    digitalWrite(_in2R, LOW);
}

static void _fowardMotors()
{
    digitalWrite(_in1L, HIGH);
    digitalWrite(_in2L, LOW);
    digitalWrite(_in1R, HIGH);
    digitalWrite(_in2R, LOW);
}

// Curva a esquerda
static void _rotateLeftMotors()
{
    digitalWrite(_in1L, LOW);
    digitalWrite(_in2L, HIGH);
    digitalWrite(_in1R, HIGH);
    digitalWrite(_in2R, LOW);
}

// Curva direita
static void _rotateRightMotors()
{
    digitalWrite(_in1L, HIGH);
    digitalWrite(_in2L, LOW);
    digitalWrite(_in1R, LOW);
    digitalWrite(_in2R, HIGH);
}

static void _esperarEncoder(long pulsos)
{ // Pra saber quando ele andou o suficiente(1 celula, 90° ou 180°)
    long baseEsq = *_encEsq;
    long baseDir = *_encDir;

    while ((*_encEsq - baseEsq) < pulsos || (*_encDir - baseDir) < pulsos)
    {
        yield();
    }
    _stopMotors();
}

// -- Atualização de posição e direção -------------------------------------------
// (renomeado de `_dirs` para `_dirsMotor` na centralização — o DFS tem o seu)
static const char _dirsMotor[] = {'N', 'L', 'S', 'O'};

static int _indiceDirecao(char d)
{
    for (int i = 0; i < 4; i++)
        if (_dirsMotor[i] == d)
            return i;
    return 0;
}

static void _atualizaDirecao(Rato *rato, int delta)
{
    rato->direcao = _dirsMotor[(_indiceDirecao(rato->direcao) + delta + 4) % 4];
}

static void _atualizaPosicao(Rato *rato)
{
    switch (rato->direcao)
    {
    case 'N':
        rato->y++;
        break;
    case 'S':
        rato->y--;
        break;
    case 'L':
        rato->x++;
        break;
    case 'O':
        rato->x--;
        break;
    }
}

// -- Movimentos públicos (antigos) -----------------------------------------------
void Andar(Rato *rato)
{
    _fowardMotors();
    _esperarEncoder(PULSOS_POR_CELULA);
    _atualizaPosicao(rato);

    // Salva contagem total nos campos do micromouse
    rato->encoder_esquerdo = *_encEsq;
    rato->encoder_direito = *_encDir;
}

void VirarEsquerda(Rato *rato)
{
    _rotateLeftMotors();
    _esperarEncoder(PULSOS_GIRO_90);
    _atualizaDirecao(rato, -1); // N→O→S→L→N (anti-horário)
}

void VirarDireita(Rato *rato)
{
    _rotateRightMotors();
    _esperarEncoder(PULSOS_GIRO_90);
    _atualizaDirecao(rato, +1); // N→L→S→O→N (horário)
}

void Virar180(Rato *rato)
{
    _rotateRightMotors();
    _esperarEncoder(PULSOS_GIRO_90 * 2);
    _atualizaDirecao(rato, +2); // N↔S | L↔O
}

// -- Configurações PWM, odometria e controle PID ---------------------------------

// Configurações PWM da ESP32
const int FREQUENCIA_PWM = 5000;
const int RESOLUCAO_PWM = 8;
const int CANAL_ESQ_IN1 = 0; const int CANAL_ESQ_IN2 = 1;
const int CANAL_DIR_IN1 = 2; const int CANAL_DIR_IN2 = 3;

// Constantes de Manobra
const int TEMPO_CURVA_90 = 600;
const int VELOCIDADE_GIRO = 150;

int velocidadeEsquerdaAtual = 0;
int velocidadeDireitaAtual = 0;
bool motorsRunning = false;

// -- Física do robô e constantes de odometria ------------------------------------
const float DIAMETRO_RODA_CM = 4.4;
const float PULSOS_POR_VOLTA = 146.0;
const float CM_POR_PULSO = (DIAMETRO_RODA_CM * PI) / PULSOS_POR_VOLTA;
// parâmetros de centralização (PAREDE_VISIVEL_CM, KP_PAREDE_PWM etc.): seção 4

// Variáveis para cálculo de velocidade
float velocidadeEsqCmS = 0.0;
float velocidadeDirCmS = 0.0;
float distanciaPercorridaCm = 0.0;

long ultimosPulsosEsq = 0;
long ultimosPulsosDir = 0;
unsigned long ultimoTempoOdometria = 0;

// -- Constantes do PID ------------------------------------------------------------
float Kp = 1.2; // Corrige o erro atual (força da correção de desvio)
float Kd = 0.5; // Evita que o robô balance (freio da correção)
float Ki = 0.0; // Corrige erros acumulados (geralmente começa em 0)

long erroAnteriorPID = 0;
long integralPID = 0;

// -- Funções PWM base ---------------------------------------------------------------
void stopMotors(); // (protótipo antecipado; em arquivos separados vinha do motor.h)

void setupMotores()
{
    ledcSetup(CANAL_ESQ_IN1, FREQUENCIA_PWM, RESOLUCAO_PWM); ledcAttachPin(_in1L, CANAL_ESQ_IN1);
    ledcSetup(CANAL_ESQ_IN2, FREQUENCIA_PWM, RESOLUCAO_PWM); ledcAttachPin(_in2L, CANAL_ESQ_IN2);
    ledcSetup(CANAL_DIR_IN1, FREQUENCIA_PWM, RESOLUCAO_PWM); ledcAttachPin(_in1R, CANAL_DIR_IN1);
    ledcSetup(CANAL_DIR_IN2, FREQUENCIA_PWM, RESOLUCAO_PWM); ledcAttachPin(_in2R, CANAL_DIR_IN2);
    stopMotors();
}

void acionarMotores(int velEsquerda, int velDireita)
{
    velocidadeEsquerdaAtual = velEsquerda;
    velocidadeDireitaAtual = velDireita;

    // Motor Esquerdo
    if (velEsquerda > 0) {
        ledcWrite(CANAL_ESQ_IN1, velEsquerda); ledcWrite(CANAL_ESQ_IN2, 0);
    } else if (velEsquerda < 0) {
        ledcWrite(CANAL_ESQ_IN1, 0); ledcWrite(CANAL_ESQ_IN2, abs(velEsquerda));
    } else {
        ledcWrite(CANAL_ESQ_IN1, 0); ledcWrite(CANAL_ESQ_IN2, 0);
    }

    // Motor Direito
    if (velDireita > 0) {
        ledcWrite(CANAL_DIR_IN1, velDireita); ledcWrite(CANAL_DIR_IN2, 0);
    } else if (velDireita < 0) {
        ledcWrite(CANAL_DIR_IN1, 0); ledcWrite(CANAL_DIR_IN2, abs(velDireita));
    } else {
        ledcWrite(CANAL_DIR_IN1, 0); ledcWrite(CANAL_DIR_IN2, 0);
    }

    motorsRunning = (velEsquerda != 0 || velDireita != 0);
}

void stopMotors() { acionarMotores(0, 0); }
void moveForward() { acionarMotores(140, 140); }

void virarDireita90()
{
    acionarMotores(VELOCIDADE_GIRO, -VELOCIDADE_GIRO);
    delay(TEMPO_CURVA_90);
    stopMotors();
    delay(200);
}

void virarEsquerda90()
{
    acionarMotores(-VELOCIDADE_GIRO, VELOCIDADE_GIRO);
    delay(TEMPO_CURVA_90);
    stopMotors();
    delay(200);
}

void meiaVolta180()
{
    acionarMotores(VELOCIDADE_GIRO, -VELOCIDADE_GIRO);
    delay(TEMPO_CURVA_90 * 2);
    stopMotors();
    delay(200);
}

// -- Correção reativa para centralização junto às paredes ---------------------------
static void _corrigirCentralizacao(int &pwmEsq, int &pwmDir)
{
    bool emCurva = (velocidadeEsquerdaAtual != velocidadeDireitaAtual);
    aplicarCorrecaoParede(getDistanciaEsquerda(), getDistanciaDireita(), pwmEsq, pwmDir, emCurva);
}

// -- Cálculo de velocidade e alinhamento PID em tempo real ---------------------------
void atualizarOdometriaEPID()
{
    unsigned long tempoAtual = millis();
    unsigned long deltaTempo = tempoAtual - ultimoTempoOdometria;

    // Calcula a cada 50ms para ter uma leitura limpa dos pulsos
    if (deltaTempo >= 50) {

        long pulsosAtuaisEsq = *_encEsq;
        long pulsosAtuaisDir = *_encDir;

        long deltaPulsosEsq = pulsosAtuaisEsq - ultimosPulsosEsq;
        long deltaPulsosDir = pulsosAtuaisDir - ultimosPulsosDir;

        // Calcula a velocidade (v = distância / tempo) em cm/s
        velocidadeEsqCmS = (deltaPulsosEsq * CM_POR_PULSO) / (deltaTempo / 1000.0);
        velocidadeDirCmS = (deltaPulsosDir * CM_POR_PULSO) / (deltaTempo / 1000.0);

        ultimosPulsosEsq = pulsosAtuaisEsq;
        ultimosPulsosDir = pulsosAtuaisDir;
        ultimoTempoOdometria = tempoAtual;

        // Só aplica PID se o robô estiver mandado a ir a direito
        if (velocidadeEsquerdaAtual > 0 && velocidadeDireitaAtual > 0) {

            // O erro é a diferença de pulsos totais desde que começou a andar
            long erro = pulsosAtuaisEsq - pulsosAtuaisDir;

            integralPID += erro;
            long derivativa = erro - erroAnteriorPID;
            erroAnteriorPID = erro;

            // Fórmula do PID
            int ajustePWM = (Kp * erro) + (Ki * integralPID) + (Kd * derivativa);

            int basePWM = 140; // O nosso PWM base de aceleração

            // Aplica a correção: Se o esquerdo corre mais, o ajustePWM é positivo,
            // logo diminui o Esq e aumenta o Dir
            int novoPwmEsq = basePWM - ajustePWM;
            int novoPwmDir = basePWM + ajustePWM;

            // Ajusta a direção com base nas distâncias dos sensores laterais
            _corrigirCentralizacao(novoPwmEsq, novoPwmDir);

            // Trava os valores para não ultrapassar a energia máxima (0 a 255)
            novoPwmEsq = constrain(novoPwmEsq, 0, 255);
            novoPwmDir = constrain(novoPwmDir, 0, 255);

            // Envia a energia corrigida direto para a Ponte H
            ledcWrite(CANAL_ESQ_IN1, novoPwmEsq);
            ledcWrite(CANAL_DIR_IN1, novoPwmDir);
        }
    }
}

// -- Função inteligente: andar distância com PID --------------------------------------
void andarDistancia(float distanciaCm)
{
    // Transforma os centímetros que queremos andar em pulsos de encoder
    long pulsosAlvo = distanciaCm / CM_POR_PULSO;

    long baseEsq = *_encEsq;
    long baseDir = *_encDir;

    // Reseta o PID antes de cada viagem para não trazer lixo da viagem anterior
    erroAnteriorPID = 0;
    integralPID = 0;

    moveForward(); // Dá o arranque inicial a direito (PWM 140,140)

    // Fica a rodar neste loop (enquanto corrige a direção) até atingir o número
    // de pulsos desejado
    while ((*_encEsq - baseEsq) < pulsosAlvo || (*_encDir - baseDir) < pulsosAlvo) {
        atualizarOdometriaEPID(); // Aciona o PID para manter as rodas alinhadas
        yield(); // Impede o Watchdog Timer da ESP32 de reiniciar o robô por loop preso
    }

    stopMotors(); // Trava a direito quando bate a meta
}

// =====================================================================
//  8. ENERGIA (INA219) — lib/input/energia/energia.h + .cpp
// =====================================================================
void inicializaIna(Adafruit_INA219 *ina219)
{
    if (!ina219->begin())
    {
        Serial.println("Erro ao iniciar INA219");
    }
    else
    {
        Serial.println("INA219 iniciado");
    }
}

void lerDadosEnergeticos(Rato *rato, Adafruit_INA219 *ina219)
{
    rato->tensao = ina219->getBusVoltage_V();
    rato->corrente = ina219->getCurrent_mA();
}

// =====================================================================
//  9. CONEXÕES WiFi/MQTT — lib/utils/conexao/conexoes.h + .cpp
//     (os stubs de ambiente nativo do conexoes.h original foram omitidos:
//      este arquivo único destina-se ao build esp32dev/Arduino)
// =====================================================================
const char *WIFI_SSID = "SUA_REDE_WIFI";      // troque para o wifi conectado ao computador e à ESP32
const char *WIFI_PASSWORD = "SUA_SENHA_WIFI"; // sua senha

const char *MQTT_BROKER = "192.168.x.x"; // IP do broker MQTT (Docker) na sua rede
const int MQTT_PORT = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.print("Conectando ao WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nWiFi conectado! IP: %s\n", WiFi.localIP());
}

void connectMQTT()
{
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    if (!mqttClient.connected())
    {
        Serial.print("Tentando conexão MQTT...");
        if (mqttClient.connect("ESP32Client"))
        {
            Serial.println("Conectado com sucesso!");
        }
        else
        {
            Serial.print("Falha, rc=");
            Serial.println(mqttClient.state());
        }
    }
}

// =====================================================================
//  10. DFS — lib/utils/dfs/dfs.h + dfs.cpp
// =====================================================================
// -------------------------------------------------------------------------------
//  ESTADOS DO ROBÔ
//
//  PARADO --> EXPLORANDO --> CORRIDA --> CONCLUIDO
//
//  EXPLORANDO : DFS (desconhecido = sem parede)
//               robô vai de INICIO descobrindo o labirinto até achar o centro 2x2
//  CORRIDA    : usa o caminho ótimo (FloodFill) — implementado depois
// -------------------------------------------------------------------------------
enum Estado
{
    PARADO,
    EXPLORANDO,
    CORRIDA,
    CONCLUIDO
};

// Posição usada na pilha de backtrack (memória estática, sem heap)
typedef struct PosicaoDFS
{
    int x;
    int y;
} PosicaoDFS;

// -- Estado interno do DFS (escopo de arquivo para permitir reset via resetDFS()) --
// Pilha do trilho percorrido. Invariante: _pilha[_topo] == célula atual do robô.
static PosicaoDFS _pilha[LAB_TAM * LAB_TAM];
static int _topo = -1;
static bool _inicializado = false;

// Zera o estado interno do DFS (pilha de backtrack e flag de inicialização).
// Deve ser chamada antes de (re)iniciar a exploração — ex.: no setup().
void resetDFS()
{
    _topo = -1;
    _inicializado = false;
}

// -- Direções ---------------------------------------------------------------------
//  Ordem horária (mesma da seção de motores): N(0) -> L(1) -> S(2) -> O(3)
//  Coordenadas: Norte -> y+1 | Sul -> y-1 | Leste -> x+1 | Oeste -> x-1
static const char _dirs[] = {'N', 'L', 'S', 'O'};

static int _indiceDir(char d)
{
    for (int i = 0; i < 4; i++)
        if (_dirs[i] == d)
            return i;
    return 0;
}

// Direção absoluta da célula vizinha relativa à orientação atual do robô
//   relativa = 0 -> frente | 1 -> direita | 3 -> esquerda
static char _dirRelativa(char direcaoAtual, int relativa)
{
    return _dirs[(_indiceDir(direcaoAtual) + relativa + 4) % 4];
}

// Vizinho na direção absoluta informada
static void _vizinho(int x, int y, char dir, int *nx, int *ny)
{
    *nx = x;
    *ny = y;
    switch (dir)
    {
    case 'N':
        (*ny)++;
        break;
    case 'S':
        (*ny)--;
        break;
    case 'L':
        (*nx)++;
        break;
    case 'O':
        (*nx)--;
        break;
    }
}

// Há parede registrada na célula (x,y) na direção informada?
static bool _temParede(Labirinto *lab, int x, int y, char dir)
{
    switch (dir)
    {
    case 'N':
        return lab->celula[x][y].norte;
    case 'S':
        return lab->celula[x][y].sul;
    case 'L':
        return lab->celula[x][y].leste;
    case 'O':
        return lab->celula[x][y].oeste;
    }
    return true;
}

// Direção absoluta para ir de (x,y) até a célula adjacente (tx,ty)
static char _direcaoEntre(int x, int y, int tx, int ty)
{
    if (tx == x + 1)
        return 'L';
    if (tx == x - 1)
        return 'O';
    if (ty == y + 1)
        return 'N';
    return 'S'; // ty == y - 1
}

// -- Último movimento (para telemetria) ---------------------------------------------
static const char *_ultimoMovimento = "parado";

// Descrição textual do último movimento executado pelo DFS.
// Valores: "frente" | "curva_a_direita" | "curva_a_esquerda" | "meia_volta" | "parado"
const char *getUltimoMovimentoDFS()
{
    return _ultimoMovimento;
}

// Gira o robô para encarar a direção absoluta 'alvo' e devolve o descritor do giro.
// O deslocamento em si (Andar) é feito por quem chama.
static const char *_virarPara(Rato *rato, char alvo)
{
    int diff = (_indiceDir(alvo) - _indiceDir(rato->direcao) + 4) % 4;
    switch (diff)
    {
    case 1:
        VirarDireita(rato);
        return "curva_a_direita";
    case 2:
        Virar180(rato);
        return "meia_volta";
    case 3:
        VirarEsquerda(rato);
        return "curva_a_esquerda";
    default: // 0 — já está na direção certa
        return "frente";
    }
}

// -- Detecção do centro 2x2 -----------------------------------------------------------
//  O objetivo é um bloco 2x2 de células totalmente aberto entre si (sem paredes
//  internas). Verificamos os 4 blocos 2x2 que contêm a célula atual. Exigimos que
//  as 4 células do bloco já tenham sido EXPLORADAS — caso contrário a detecção
//  dispararia logo no início, já que o labirinto inicia sem paredes internas.
static bool _verificaCentro(Labirinto *lab, int x, int y, int *destinoX, int *destinoY)
{
    // cantos inferior-esquerdos (bx,by) dos blocos 2x2 que incluem (x,y)
    const int cantos[4][2] = {{x, y}, {x - 1, y}, {x, y - 1}, {x - 1, y - 1}};

    for (int i = 0; i < 4; i++)
    {
        int bx = cantos[i][0];
        int by = cantos[i][1];

        if (bx < 0 || by < 0 || bx >= LAB_TAM - 1 || by >= LAB_TAM - 1)
            continue;

        // as 4 células do bloco precisam ter sido visitadas/exploradas
        if (!lab->celula[bx][by].explorado ||
            !lab->celula[bx + 1][by].explorado ||
            !lab->celula[bx][by + 1].explorado ||
            !lab->celula[bx + 1][by + 1].explorado)
            continue;

        // paredes internas do bloco 2x2 ausentes:
        //   leste de (bx,by)   e leste de (bx,by+1)  -> divisória vertical aberta
        //   norte de (bx,by)   e norte de (bx+1,by)  -> divisória horizontal aberta
        bool aberto =
            !lab->celula[bx][by].leste &&
            !lab->celula[bx][by + 1].leste &&
            !lab->celula[bx][by].norte &&
            !lab->celula[bx + 1][by].norte;

        if (aberto)
        {
            *destinoX = bx;
            *destinoY = by;
            return true;
        }
    }
    return false;
}

// -- Passo de DFS (iterativo, pilha explícita estática) ---------------------------------
//  Executa EXATAMENTE 1 passo por chamada (compatível com loop() não-bloqueante).
//  Cada passo: lê sensores, registra paredes, marca célula, decide e move 1 célula
//  (ou faz backtrack). Não usa recursão nem heap — pilha estática explícita.
//  Prioridade de movimento: frente -> direita -> esquerda -> backtrack
void passoDFS(Rato *rato, Labirinto *lab, bool *motorsRunning,
              unsigned long *stepCounter, int *destinoX, int *destinoY,
              bool *conclusao, Estado *estado)
{
    if (!_inicializado)
    {
        _topo = 0;
        _pilha[0].x = rato->x;
        _pilha[0].y = rato->y;
        _inicializado = true;
    }

    // 1) Sensores
    atualizaSensores();
    lerDistancias(rato);

    // 2) Registrar paredes vistas na célula atual (converte relativo -> absoluto)
    char dirFrente = _dirRelativa(rato->direcao, 0);
    char dirDireita = _dirRelativa(rato->direcao, 1);
    char dirEsquerda = _dirRelativa(rato->direcao, 3);

    if (temParedeFrente())
        registrarParede(lab, rato->x, rato->y, dirFrente);
    if (temParedeDireita())
        registrarParede(lab, rato->x, rato->y, dirDireita);
    if (temParedeEsquerda())
        registrarParede(lab, rato->x, rato->y, dirEsquerda);

    // 3) Marcar célula atual
    lab->celula[rato->x][rato->y].explorado = true;
    lab->celula[rato->x][rato->y].visitado = true;

    // 4) Contabilizar o passo
    (*stepCounter)++;

    // 5) Chegou no centro 2x2?
    if (_verificaCentro(lab, rato->x, rato->y, destinoX, destinoY))
    {
        *conclusao = true;
        *motorsRunning = false;
        *estado = CONCLUIDO;
        _ultimoMovimento = "parado";
        return;
    }

    // 6/7) Escolher próximo movimento: frente -> direita -> esquerda
    //      Só anda para vizinha que NÃO foi visitada e sem parede entre elas.
    char ordem[3] = {dirFrente, dirDireita, dirEsquerda};
    for (int i = 0; i < 3; i++)
    {
        char dir = ordem[i];

        if (_temParede(lab, rato->x, rato->y, dir))
            continue;

        int nx, ny;
        _vizinho(rato->x, rato->y, dir, &nx, &ny);

        if (nx < 0 || ny < 0 || nx >= LAB_TAM || ny >= LAB_TAM)
            continue;
        if (lab->celula[nx][ny].visitado)
            continue;

        // Vizinha válida e inédita -> vira, anda e empilha a nova posição
        _ultimoMovimento = _virarPara(rato, dir);
        Andar(rato); // atualiza rato->x / rato->y
        *motorsRunning = true;

        if (_topo < LAB_TAM * LAB_TAM - 1)
        {
            _topo++;
            _pilha[_topo].x = rato->x;
            _pilha[_topo].y = rato->y;
        }
        return;
    }

    // 8) Sem vizinhas inéditas -> backtrack (volta para o ponto anterior do trilho)
    _topo--; // desempilha a célula atual

    if (_topo < 0)
    {
        // 9) Pilha vazia: exploração completa sem encontrar o centro
        *motorsRunning = false;
        *estado = CONCLUIDO;
        _ultimoMovimento = "parado";
        return;
    }

    PosicaoDFS anterior = _pilha[_topo];
    char dirVolta = _direcaoEntre(rato->x, rato->y, anterior.x, anterior.y);
    _ultimoMovimento = _virarPara(rato, dirVolta);
    Andar(rato); // volta uma célula; rato->x/y passam a ser os de 'anterior'
    *motorsRunning = true;
}

// =====================================================================
//  11. TELEMETRIA — lib/utils/telemetria/telemetria.h + .cpp
// =====================================================================
static const char *_estadoParaDto(Estado estado)
{
    switch (estado)
    {
    case CONCLUIDO:
        return "FINALIZADO";
    case CORRIDA:
        return "EXPLORANDO";
    case EXPLORANDO:
        return "EXPLORANDO";
    case PARADO:
    default:
        return "PARADO";
    }
}

void publishTelemetry(
    const Rato &rato,
    const Labirinto &lab,
    PubSubClient &mqttClient,
    const char *mqttTopic,
    const char *robotId,
    unsigned long &stepCounter,
    Estado estado,
    bool motorsRunning,
    const char *ultimoMovimento,
    bool concluded)
{
    if (WiFi.status() != WL_CONNECTED || !mqttClient.connected())
        return;

    StaticJsonDocument<768> doc;

    doc["robotId"] = robotId;
    doc["step"] = stepCounter; // incremento fica a cargo do passoDFS()
    doc["tempoMs"] = millis();
    doc["modo"] = "DFS";
    doc["estado"] = _estadoParaDto(estado);

    JsonObject posicao = doc.createNestedObject("posicao");
    posicao["x"] = rato.x;
    posicao["y"] = rato.y;

    const char *dirStr = rato.direcao == 'N' ? "norte" : rato.direcao == 'S' ? "sul"
                                                     : rato.direcao == 'L'   ? "leste"
                                                                             : "oeste";
    doc["direcao"] = dirStr;

    doc["ultimoMovimento"] = ultimoMovimento;

    JsonObject paredes = doc.createNestedObject("paredes");
    paredes["norte"] = lab.celula[rato.x][rato.y].norte;
    paredes["sul"] = lab.celula[rato.x][rato.y].sul;
    paredes["leste"] = lab.celula[rato.x][rato.y].leste;
    paredes["oeste"] = lab.celula[rato.x][rato.y].oeste;

    JsonObject motores = doc.createNestedObject("motores");
    motores["pwmEsquerdo"] = rato.pwm_motor_esquerdo;
    motores["pwmDireito"] = rato.pwm_motor_direito;

    JsonObject sensores = doc.createNestedObject("sensores");
    sensores["esquerdaCm"] = rato.distancia_esquerda;
    sensores["frenteCm"] = rato.distancia_frente;
    sensores["direitaCm"] = rato.distancia_direita;

    JsonObject energia = doc.createNestedObject("energia");
    energia["tensaoV"] = rato.tensao;
    energia["correnteMa"] = rato.corrente;

    doc["conclusao"] = concluded;

    char buffer[512];
    serializeJson(doc, buffer);
    mqttClient.publish(mqttTopic, buffer);
}

// =====================================================================
//  12. PROGRAMA PRINCIPAL — src/main.cpp
// =====================================================================
#pragma region Variáveis

const char *MQTT_TOPIC = "rato/telemetria";
const char *ROBOT_ID = "UAV-MOUSE-01";

// -------------------------------------------------------------------------------
//  PINOS
// -------------------------------------------------------------------------------
const uint8_t LED_PIN = 2;

// INA219
const uint8_t INA219_SDA = 21;
const uint8_t INA219_SCL = 15;
Adafruit_INA219 ina219;

// Sensores Infravermelho (VL53L0X + VL6180X) - pinos XSHUT
const uint8_t XSHUT_FRONT = 4;
const uint8_t XSHUT_LEFT = 17;
const uint8_t XSHUT_RIGHT = 18;

// Motores
const uint8_t MOTOR_LEFT_IN1 = 25;
const uint8_t MOTOR_LEFT_IN2 = 26;
const uint8_t MOTOR_RIGHT_IN1 = 27;
const uint8_t MOTOR_RIGHT_IN2 = 14;

// Encoders
const uint8_t ENCODER_LEFT_A = 34;
const uint8_t ENCODER_LEFT_B = 35;
const uint8_t ENCODER_RIGHT_A = 32;
const uint8_t ENCODER_RIGHT_B = 33;

// -------------------------------------------------------------------------------
//  LABIRINTO - posições de início e destino
// -------------------------------------------------------------------------------
#define INICIO_X 15
#define INICIO_Y 15

// vou usar dps pra floodfill (-1 pq não encontrou ainda)
int destinoX = -1;
int destinoY = -1;

// -------------------------------------------------------------------------------
//  ESTADOS
//
//  PARADO --> EXPLORANDO --> EXPLORANDO (Volta) --> CORRIDA --> CONCLUIDO
//
//  EXPLORANDO : DFS (desconhecido = sem parede)
//               robô vai de INICIO até DEST descobrindo o labirinto e a posição final
//                     --> Volta pro começo usando FloodFill
//
//  CORRIDA  : Usa caminho descoberto no Floodfill
//              --> robô volta de DEST a INICIO pelo melhor caminho
//
//  O enum Estado é definido na seção 10 (DFS).
// -------------------------------------------------------------------------------
Estado estado = PARADO;

// -------------------------------------------------------------------------------
//  GLOBAIS
// -------------------------------------------------------------------------------
// Variáveis Voláteis para Interrupções
volatile long encoderLeftCount = 0;
volatile long encoderRightCount = 0;

// Gerenciamento de Timers Assíncronos
unsigned long lastTelemetrySend = 0;
unsigned long lastMotorToggle = 0;
unsigned long lastLedBlink = 0;
unsigned long lastSerialLog = 0;

bool ledState = false;
bool concluido = false;
unsigned long stepCounter = 0;

Rato rato;
Labirinto lab;

// wifiClient e mqttClient são definidos na seção 9 (conexões)

#pragma endregion

#pragma region Encoders
// -------------------------------------------------------------------------------
//  ISRs - ENCODERS
// -------------------------------------------------------------------------------
void IRAM_ATTR encoderLeftISR()
{
    encoderLeftCount++;
}

void IRAM_ATTR encoderRightISR()
{
    encoderRightCount++;
}

#pragma endregion

// -------------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------------
#pragma region Setup

void setup()
{
    Serial.begin(115200);
    mqttClient.setBufferSize(1024);

    // LED
    pinMode(LED_PIN, OUTPUT);

    // Encoders - ISRs definidas neste arquivo, ponteiros passados para os motores
    pinMode(ENCODER_LEFT_A, INPUT_PULLUP);
    pinMode(ENCODER_RIGHT_A, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A), encoderLeftISR, RISING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_A), encoderRightISR, RISING);

    // Sensores Infravermelho (I2C - XSHUT)
    inicializaSensores(XSHUT_FRONT, XSHUT_LEFT, XSHUT_RIGHT);
    // Motores (pinos + referência aos contadores de encoder)
    inicializaMotores(MOTOR_LEFT_IN1, MOTOR_LEFT_IN2,
                      MOTOR_RIGHT_IN1, MOTOR_RIGHT_IN2,
                      &encoderLeftCount, &encoderRightCount);

    inicializaIna(&ina219);

    inicializaRato(&rato);

    inicializaLabirinto(&lab);

    // Rede
    connectWiFi();
    connectMQTT();

    delay(1000); // só um tempo pra começar dps

    resetDFS();          // garante pilha/flags zeradas antes de explorar
    estado = EXPLORANDO; // inicia a exploração por DFS
}
#pragma endregion

// -------------------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------------------
#pragma region Loop

void loop()
{

    if (WiFi.status() != WL_CONNECTED)
        connectWiFi();
    if (!mqttClient.connected())
        connectMQTT();

    mqttClient.loop();

    unsigned long currentMillis = millis();

    // Telemetria MQTT (2s)
    if (currentMillis - lastTelemetrySend >= 2000)
    {
        lastTelemetrySend = currentMillis;

        lerDadosEnergeticos(&rato, &ina219);
        publishTelemetry(rato, lab, mqttClient, MQTT_TOPIC, ROBOT_ID, stepCounter, estado, motorsRunning, getUltimoMovimentoDFS(), concluido);
    }

    // Serial (2s)
    if (currentMillis - lastSerialLog >= 2000)
    {
        lastSerialLog = currentMillis;
        Serial.println("\n--- [TELEMETRIA LOCAL] ---");
        Serial.printf("Distâncias -> F: %.2f cm | E: %.2f cm | D: %.2f cm\n", rato.distancia_frente, rato.distancia_esquerda, rato.distancia_direita);
        Serial.printf("Encoders   -> L: %ld | R: %ld\n", encoderLeftCount, encoderRightCount);
        Serial.printf("Motores    -> Status: %s\n", motorsRunning ? "EM MOVIMENTO" : "PARADO");
    }

    // Ações baseadas no Estado do Robô
    switch (estado)
    {
    case EXPLORANDO:
    {
        // 1. Lê o ambiente (Apenas uma vez!)
        atualizaSensores();

        Serial.printf("[CÉREBRO] Distâncias -> Frente: %.1f | Esq: %.1f | Dir: %.1f\n",
                      rato.distancia_frente, rato.distancia_esquerda, rato.distancia_direita);

        // 2. Lógica de Decisão
        if (rato.distancia_frente > 15.0) {
            Serial.println("[AÇÃO] Caminho livre! A avançar 1 célula (18cm)...");
            andarDistancia(18.0);

            // Atualiza o cérebro! (Se está virado para Norte, o Y sobe, etc.)
            if (rato.direcao == 'N') rato.y++;
            else if (rato.direcao == 'S') rato.y--;
            else if (rato.direcao == 'L') rato.x++;
            else if (rato.direcao == 'O') rato.x--;

            stepCounter++; // Regista que deu um passo!
            delay(300);
        }
        else {
            if (rato.distancia_esquerda > 15.0) {
                Serial.println("[AÇÃO] A virar à Esquerda 90°.");
                virarEsquerda90();
                // Atualiza a bússola do rato (Anti-horário)
                if (rato.direcao == 'N') rato.direcao = 'O';
                else if (rato.direcao == 'O') rato.direcao = 'S';
                else if (rato.direcao == 'S') rato.direcao = 'L';
                else if (rato.direcao == 'L') rato.direcao = 'N';
            }
            else if (rato.distancia_direita > 15.0) {
                Serial.println("[AÇÃO] A virar à Direita 90°.");
                virarDireita90();
                // Atualiza a bússola do rato (Horário)
                if (rato.direcao == 'N') rato.direcao = 'L';
                else if (rato.direcao == 'L') rato.direcao = 'S';
                else if (rato.direcao == 'S') rato.direcao = 'O';
                else if (rato.direcao == 'O') rato.direcao = 'N';
            }
            else {
                Serial.println("[AÇÃO] Beco sem saída. A dar meia-volta 180°.");
                meiaVolta180();
                // Inverte a bússola
                if (rato.direcao == 'N') rato.direcao = 'S';
                else if (rato.direcao == 'S') rato.direcao = 'N';
                else if (rato.direcao == 'L') rato.direcao = 'O';
                else if (rato.direcao == 'O') rato.direcao = 'L';
            }
        }
        break;
    }

    case CORRIDA:
        // FloodFill — não implementar agora
        stopMotors();
        break;

    case CONCLUIDO:
    case PARADO:
    default:
        stopMotors();
        break;
    }
}

#pragma endregion