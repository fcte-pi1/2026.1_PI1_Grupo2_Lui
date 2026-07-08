#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Wire.h>

#include "../pins.h"
#include "../distanceSensor/distanceSensor.h"
#include "../motors/motors.h"
#include "../movimento/movimento.h"

BluetoothSerial SerialBT;

static String comandoSerial = "";
static String comandoBT = "";
static bool testeFrenteAtivo = false;
static bool leituraContinua = false;
static unsigned long ultimoLog = 0;

static constexpr int VEL_TESTE_FRENTE = 40;
static constexpr unsigned long INTERVALO_LOG_MS = 300;

void logMsg(const String& msg) {
  Serial.println(msg);
  if (SerialBT.hasClient()) {
    SerialBT.println(msg);
  }
}

static String ladosEmergenciaTexto(uint8_t lados) {
  String texto = "";
  if (lados & EMERGENCIA_TOF_ESQ) texto += "ESQ ";
  if (lados & EMERGENCIA_TOF_FRENTE) texto += "FRENTE ";
  if (lados & EMERGENCIA_TOF_DIR) texto += "DIR ";
  if (texto.length() == 0) texto = "nenhum";
  return texto;
}

static void imprimirLeituras() {
  atualizar_filtro_media();
  verificar_emergencia();

  char buf[160];
  snprintf(
      buf,
      sizeof(buf),
      "TOF | Esq:%4u mm | Frente:%4u mm | Dir:%4u mm | emergencia:%s | lados:%s",
      distancia_esquerda_mm(),
      distancia_frente_mm(),
      distancia_direita_mm(),
      emergencia_ativa() ? "SIM" : "NAO",
      ladosEmergenciaTexto(emergencia_tof_lados()).c_str());
  logMsg(String(buf));
}

static void pararTeste() {
  testeFrenteAtivo = false;
  motors_stop_all();
  logMsg("STOP: motores travados.");
}

static void iniciarTesteFrente() {
  atualizar_filtro_media();
  if (verificar_emergencia()) {
    logMsg("FWD bloqueado: emergency stop ja esta ativo. Afaste o robo ou use RECOVER.");
    imprimirLeituras();
    return;
  }

  testeFrenteAtivo = true;
  motor_esquerdo_set(VEL_TESTE_FRENTE);
  motor_direito_set(VEL_TESTE_FRENTE);
  logMsg("FWD: motores ligados para frente em 40%. Aproxime uma parede de qualquer ToF; deve parar em <= 20 mm.");
}

static void executarRecover() {
  logMsg("RECOVER: afastando para distancia segura...");
  recuperar_centro_labirinto();
  imprimirLeituras();
  logMsg(emergencia_ativa() ? "RECOVER terminou, mas emergencia segue ativa." : "RECOVER terminou com emergencia liberada.");
}

static void mostrarMenu() {
  logMsg("");
  logMsg("===== TESTE EMERGENCY STOP TOF =====");
  logMsg("READ     -> imprime uma leitura dos 3 ToFs");
  logMsg("TOF_ON   -> imprime leituras continuamente a cada 300 ms");
  logMsg("TOF_OFF  -> para leituras continuas");
  logMsg("FWD      -> anda devagar ate emergency stop disparar");
  logMsg("RECOVER  -> tenta afastar o robo da parede e rearma a trava");
  logMsg("STOP     -> para os motores agora");
  logMsg("HELP     -> mostra este menu");
  logMsg("Limite emergency stop: 20 mm. Rearme: 50 mm.");
  logMsg("====================================");
}

static void executarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  logMsg(">> " + cmd);

  if (cmd == "READ") {
    imprimirLeituras();
  } else if (cmd == "TOF_ON") {
    leituraContinua = true;
  } else if (cmd == "TOF_OFF") {
    leituraContinua = false;
  } else if (cmd == "FWD") {
    iniciarTesteFrente();
  } else if (cmd == "RECOVER") {
    executarRecover();
  } else if (cmd == "STOP") {
    pararTeste();
  } else if (cmd == "HELP" || cmd == "?") {
    mostrarMenu();
  } else {
    logMsg("Comando desconhecido. Use HELP.");
  }
}

static void lerComandos(Stream& entrada, String& buffer) {
  while (entrada.available()) {
    char c = entrada.read();
    if (c == '\n' || c == '\r') {
      if (buffer.length() > 0) {
        executarComando(buffer);
        buffer = "";
      }
    } else {
      buffer += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  SerialBT.begin("ESP32_ESTOP_TOF");

  logMsg("");
  logMsg("Boot teste emergency stop ToF");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  motors_init();
  configurarSensoresToF();

  for (int i = 0; i < NUM_AMOSTRAS; i++) {
    atualizar_filtro_media();
    delay(25);
  }

  motors_stop_all();
  mostrarMenu();
}

void loop() {
  lerComandos(Serial, comandoSerial);
  lerComandos(SerialBT, comandoBT);

  atualizar_filtro_media();
  bool emergencia = verificar_emergencia();

  if (testeFrenteAtivo && emergencia) {
    testeFrenteAtivo = false;
    motors_stop_all();
    logMsg("EMERGENCY STOP DISPAROU: motores travados.");
    imprimirLeituras();
  }

  if ((leituraContinua || testeFrenteAtivo) && millis() - ultimoLog >= INTERVALO_LOG_MS) {
    ultimoLog = millis();
    imprimirLeituras();
  }

  delay(10);
}
