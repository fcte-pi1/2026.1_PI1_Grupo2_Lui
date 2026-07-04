#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

bool configurarMPU();

void mpu_update();

float mpu_get_yaw();

void mpu_reset_yaw();

#endif