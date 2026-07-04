#include "mpu.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

bool configurarMPU() {

  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Falha ao encontrar o chip MPU-6500! Verifique as conexões.");
    return false;
  }
  
  Serial.println("MPU-6500 inicializado com sucesso!\n");

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G); 
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);     
  
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); 
  
  delay(100);
  return true;
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

  float giroZ_graus = g.gyro.z * 57.2958f; 
  yaw_atual += giroZ_graus * dt;
}

float mpu_get_yaw() {
    return yaw_atual;
}

void mpu_reset_yaw() {
    yaw_atual = 0.0f;
    ultimo_tempo_mpu = 0;
}