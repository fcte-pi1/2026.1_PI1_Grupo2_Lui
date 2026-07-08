#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "motors/motors.h"
#include "distanceSensor/distanceSensor.h"
#include "movimento/movimento.h"
#include "mpu/mpu.h"
#include "encoder/encoder.h"
#include "centralizacao/centralizacao.h"

static constexpr int VEL_TESTE = 150;

void setup() {

    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=======================================");
    Serial.println(" BOOT Micromouse");
    Serial.println("=======================================\n");

    Serial.println("[BOOT 1/5] Inicializando Barramento I2C...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);
    Serial.println("-> I2C pronto nos pinos SDA(21) e SCL(22) @ 400kHz.\n");
    delay(200);

    Serial.println("[BOOT 2/5] Inicializando sensor de distânca VL53L0X...");
    configurarSensoresToF();
    Serial.println("-> Sensor VL53L0X inicializado.\n");
    delay(200);

    Serial.println("[BOOT 3/5] Inicializando MPU-6500...");
    configurarMPU();
    Serial.println("-> MPU-6500 inicializado.\n");
    delay(200);

    Serial.println("[BOOT 3.1/5] Calibrando offset do giroscopio (mantenha o robo parado)...");
    mpu_calibrar_offset_giro();
    Serial.println("-> Offset do giroscopio calibrado.\n");
    delay(200);

    Serial.println("[BOOT 4/5] Configurando encoders dos motores...");
    encoders_init();
    Serial.println("-> Encoders configurados nos pinos 32, 33 (motor esquerdo) e 34, 35 (motor direito).\n");
    delay(200);

    Serial.println("[BOOT 5/5] Configurando motores...");
    motors_init();
    Serial.println("-> PWM: 20 kHz, resolucao 8 bits.");
    Serial.println("-> Motores inicializados em estado de parada.\n");
    delay(2000);

    Serial.println("\n=======================================");
    Serial.println(" BOOT Completo ");
    Serial.println("=======================================\n");

    loop();
}

void loop() {

    // Atualiza o filtro de média dos sensores de distância
    atualizar_filtro_media();

    // Atualiza a integração do giroscópio (yaw)
    atualizarMPU();

    // Verifica freio de emergência
    if (verificar_emergencia()) {
        recuperar_centro_labirinto();
    }

    // Centraliza o robô no corredor usando sensores + giroscópio
    centralizar_no_corredor();

    // Imprime mock do map_manager se houve mudança no estado das paredes
    testar_sensores_paredes();

    delay(20);
}
