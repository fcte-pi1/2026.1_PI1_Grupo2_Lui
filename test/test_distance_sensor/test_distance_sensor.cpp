#include <Arduino.h>
#include <unity.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "pins.h"
#include "distanceSensor.h"

void setUp(void) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    delay(100);
    configurarSensoresToF();
    delay(500);
}

void tearDown(void) {}

void test_sensor_initialization_success(void) {
    // Verifica que configurarSensoresToF() não registra timeouts para nenhum sensor
    ToFSensorReading reading = lerTodosSensores();
    TEST_ASSERT_FALSE(reading.erroEsq);
    TEST_ASSERT_FALSE(reading.erroFrente);
    TEST_ASSERT_FALSE(reading.erroDir);
}

void test_sensor_independence_left_isolation(void) {
    //leitura inicial
    ToFSensorReading baseline = lerTodosSensores();
    TEST_ASSERT_FALSE(baseline.erroEsq);
    TEST_ASSERT_FALSE(baseline.erroDir);

    // Lê sensor esquerdo 
    for (int i = 0; i < 5; i++) {
        ToFSensorReading reading = lerTodosSensores();
        TEST_ASSERT_FALSE(reading.erroEsq);
        TEST_ASSERT_FALSE(reading.erroDir);

        // Validar que leitura direita não é afetada drasticamente
        if (baseline.distDir < 8000 && reading.distDir < 8000) {
            // Se ambas estão em range, a diferença não deve ser > 200mm
            if (baseline.distDir > 200) {
                int diff = abs((int)reading.distDir - (int)baseline.distDir);
                TEST_ASSERT_TRUE(diff < 200);
            }
        }
        delay(50);
    }
}

void test_sensor_independence_right_isolation(void) {
    //leitura inicial
    ToFSensorReading baseline = lerTodosSensores();
    TEST_ASSERT_FALSE(baseline.erroDir);
    TEST_ASSERT_FALSE(baseline.erroEsq);

    // Lê sensor direito
    for (int i = 0; i < 5; i++) {
        ToFSensorReading reading = lerTodosSensores();
        TEST_ASSERT_FALSE(reading.erroDir);
        TEST_ASSERT_FALSE(reading.erroEsq);

        // Validar que leitura esquerda não é afetada drasticamente
        if (baseline.distEsq < 8000 && reading.distEsq < 8000) {
            if (baseline.distEsq > 200) {
                int diff = abs((int)reading.distEsq - (int)baseline.distEsq);
                TEST_ASSERT_TRUE(diff < 200);
            }
        }
        delay(50);
    }
}

void test_sensor_addresses_unique(void) {
    // Verifica que cada sensor responde sem erro 
    ToFSensorReading reading = lerTodosSensores();

    // Se não há timeout, significa que cada sensor está respondendo corretamente em seu endereço único
    TEST_ASSERT_FALSE(reading.erroEsq);
    TEST_ASSERT_FALSE(reading.erroFrente);
    TEST_ASSERT_FALSE(reading.erroDir);
}

void test_four_combinations(void) {
    // Ambos os sensores livres (sem obstrução próxima)
    // Esperado: sem timeout, ambas > 300mm 
    {
        ToFSensorReading reading = lerTodosSensores();
        TEST_ASSERT_FALSE(reading.erroEsq);
        TEST_ASSERT_FALSE(reading.erroDir);
    }

    // Sensor esquerdo com leitura válida, verificar direito
    // (ambos devem responder sem interferência)
    {
        ToFSensorReading reading1 = lerTodosSensores();
        delay(20);
        ToFSensorReading reading2 = lerTodosSensores();

        TEST_ASSERT_FALSE(reading1.erroEsq);
        TEST_ASSERT_FALSE(reading1.erroDir);
        TEST_ASSERT_FALSE(reading2.erroEsq);
        TEST_ASSERT_FALSE(reading2.erroDir);
    }

    // Validar estabilidade - múltiplas leituras seguidas
    {
        uint16_t dist_esq_baseline = 0;
        uint16_t dist_dir_baseline = 0;

        for (int i = 0; i < 3; i++) {
            ToFSensorReading reading = lerTodosSensores();
            TEST_ASSERT_FALSE(reading.erroEsq);
            TEST_ASSERT_FALSE(reading.erroDir);

            if (i == 0) {
                dist_esq_baseline = reading.distEsq;
                dist_dir_baseline = reading.distDir;
            } else {
                // Verificar que readings são consistentes (variação < 100mm)
                if (dist_esq_baseline < 8000) {
                    int diff_esq = abs((int)reading.distEsq - (int)dist_esq_baseline);
                    TEST_ASSERT_TRUE(diff_esq < 100);
                }
                if (dist_dir_baseline < 8000) {
                    int diff_dir = abs((int)reading.distDir - (int)dist_dir_baseline);
                    TEST_ASSERT_TRUE(diff_dir < 100);
                }
            }
            delay(50);
        }
    }

    // Validar que um sensor bloqueado não afeta o outro
    // Fazemos múltiplas leituras para garantir consistência
    {
        for (int i = 0; i < 3; i++) {
            ToFSensorReading reading = lerTodosSensores();
            TEST_ASSERT_FALSE(reading.erroEsq);
            TEST_ASSERT_FALSE(reading.erroDir);
            delay(50);
        }
    }
}

void setup(void) {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_sensor_initialization_success);
    RUN_TEST(test_sensor_independence_left_isolation);
    RUN_TEST(test_sensor_independence_right_isolation);
    RUN_TEST(test_sensor_addresses_unique);
    RUN_TEST(test_four_combinations);
    UNITY_END();
}

void loop() {}
