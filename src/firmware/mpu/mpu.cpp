#include "mpu.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

void configurarMPU() {

  // Inicializa o MPU-6500 no endereço padrão 0x68 e atrela ao barramento Wire já existente
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Falha ao encontrar o chip MPU-6500! Verifique as conexões.");
    while (1) {
      delay(10); // Trava o sistema caso o sensor falhe
    }
  }
  
  Serial.println("MPU-6500 inicializado com sucesso!\n");

  // Configuração das escalas (Ajustadas para dinâmica de robôs de solo)
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G); // Escala de até 4G de aceleração
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);      // Escala de até 500 graus/s de rotação
  
  // Filtro passa-baixa para remover ruído de vibração dos motores
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); 
  
  delay(100); // Pequeno tempo para o sensor estabilizar
}

static float yaw_atual = 0.0f;
static unsigned long ultimo_tempo_mpu = 0;

void mpu_update() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  unsigned long tempo_agora = millis();
  if (ultimo_tempo_mpu == 0) {
      ultimo_tempo_mpu = tempo_agora;
      return;
  }

  float dt = (tempo_agora - ultimo_tempo_mpu) / 1000.0f;
  ultimo_tempo_mpu = tempo_agora;

  // Converte a rotação Z de rad/s para graus/s e integra
  float giroZ_graus = g.gyro.z * 57.2958f; 
  yaw_atual += giroZ_graus * dt;
}

float mpu_get_yaw() {
    return yaw_atual;
}

void mpu_reset_yaw() {
    yaw_atual = 0.0f;
    ultimo_tempo_mpu = 0; // Reinicia o delta time
}