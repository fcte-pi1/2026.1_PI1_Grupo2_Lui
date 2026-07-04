#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "motors/motors.h"
#include "distanceSensor/distanceSensor.h"
#include "mpu/mpu.h"
#include "encoder/encoder.h"
#include "pid/pid.h"
#include "bateria/bateria.h"
#include "telemetria/telemetria.h"
#include "movimento/movimento.h"
#include "map_manager/map_manager.h"

enum EstadoRobo {
    LIGANDO,
    VERIFICANDO,
    EXPLORANDO,
    CORRIDA,
    FINALIZADO,
    ERRO
};

EstadoRobo estado_atual = LIGANDO;
unsigned long ultimo_tempo_erro = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=======================================");
    Serial.println(" BOOT Micromouse");
    Serial.println("=======================================\n");
}

void loop() {
    switch (estado_atual) {
        case LIGANDO:
            Serial.println("[ESTADO] LIGANDO...");
            Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
            Wire.setClock(400000);
            
            encoders_init();
            motors_init();
            telemetry_init();
            map_init();
            
            // Validando Flood Fill CT-14 / CT-45
            testar_flood_fill();
            
            estado_atual = VERIFICANDO;
            break;
            
        case VERIFICANDO:
            Serial.println("[ESTADO] VERIFICANDO periféricos...");
            if (!configurarMPU()) {
                Serial.println("[ERRO] MPU falhou.");
                estado_atual = ERRO;
                break;
            }
            if (!configurarSensoresToF()) {
                Serial.println("[ERRO] Sensores ToF falharam.");
                estado_atual = ERRO;
                break;
            }
            Serial.println("[INFO] Periféricos verificados com sucesso.");
            Serial.println("[ESTADO] Entrando em EXPLORANDO...");
            estado_atual = EXPLORANDO;
            break;
            
        case EXPLORANDO:
        {
            atualizar_filtro_media();
            atualizar_velocidade();
            verificar_emergencia();
            testar_sensores_paredes();

            float bat_v = ler_tensao_bateria();
            float vel_mms = obter_velocidade_mm_s();
            telemetry_loop(bat_v, vel_mms);

            static bool simulou_parede = false;
            static unsigned long tempo_inicio_explorando = millis();
            
            if (!simulou_parede && (millis() - tempo_inicio_explorando > 5000)) {
                Serial.println("\n>>> [MOCK] Simulando movimentação e detecção de parede para teste (CT-06/CT-13) <<<");
                
                map_update_robot_pos(0, MAZE_SIZE - 2, NORTE);

                map_set_wall(0, MAZE_SIZE - 2, NORTE, true);
                
                map_print();
                simulou_parede = true;
            }
            break;
        }
        case CORRIDA:
            break;
            
        case FINALIZADO:
            break;
            
        case ERRO:
            if (millis() - ultimo_tempo_erro > 2000) {
                Serial.println("[ESTADO] ERRO: O sistema está travado por falha na inicialização.");
                ultimo_tempo_erro = millis();
            }
            break;
    }

    delay(20);
}
