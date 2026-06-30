#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "motors/motors.h"
#include "distanceSensor/distanceSensor.h"
#include "mpu/mpu.h"
#include "encoder/encoder.h"
#include "pid/pid.h"

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

    // TESTE DE MESA CT-10: Teste do PID
    Serial.println("[CT-10] Iniciando teste do PID...");
    resetar_pid();
    float erros_simulados[] = {-10.0f, 0.0f, 10.0f};
    
    for (int i = 0; i < 3; i++) {
        float erro = erros_simulados[i];
        float ajuste = calcular_pid(erro, 0.02f); // 20ms delta
        Serial.print("Erro Simulado: ");
        Serial.print(erro);
        Serial.print(" => Ajuste PID: ");
        Serial.println(ajuste);
        
        int vel_esq = VEL_TESTE + (int)ajuste;
        int vel_dir = VEL_TESTE - (int)ajuste;
        Serial.print("  Motores (Esq / Dir): ");
        Serial.print(vel_esq);
        Serial.print(" / ");
        Serial.println(vel_dir);
    }
    Serial.println("[CT-10] Teste concluído.\n");

    loop();
}

void loop() {

    // Atualiza o filtro de média dos sensores de distância
    atualizar_filtro_media();

    // Verifica freio de emergência
    verificar_emergencia();

    // Imprime mock do map_manager se houve mudança no estado das paredes
    testar_sensores_paredes();

    delay(20);
}
