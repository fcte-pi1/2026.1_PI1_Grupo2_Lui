# Estudo de Dimensionamento e Eficiência Energética - Micromouse

Este documento formaliza a análise técnica do subsistema de alimentação do robô Micromouse, detalhando o consumo individual de componentes, o cálculo de autonomia energética e as estratégias de regulação e proteção do circuito.

## 1. Identificação dos Subsistemas Elétricos

Os componentes foram mapeados conforme a tensão de operação e corrente média em regime de funcionamento nominal para um tempo estimado de operação contínua de 1800 segundos (30 minutos).

| Componente | Tensão (V) | Corrente Média (mA) | Tempo (s) |
| :--- | :---: | :---: | :---: |
| Microcontrolador ESP32 | 5.0V | 240 mA | 1800 s |
| Motores N20 (Par) | 7.4V | 200 mA | 1800 s |
| Sensor ToF (VL53L0X 3x) | 5.0V | 45 mA | 1800 s |
| Giroscópio MPU6500 | 5.0V | 5 mA | 1800 s |

## 2. Cálculo da Energia Consumida por Componente

A energia (E) consumida por cada componente é calculada pela fórmula **E = V · I · t**, onde a corrente (I) é expressa em Ampères.

* **ESP32:** 5V × 0.24A × 1800s = 2160 J
* **Motores N20:** 7.4V × 0.20A × 1800s = 2664 J
* **Sensor ToF (3x):** 5V × 0.045A × 1800s = 405 J
* **Giroscópio:** 5V × 0.005A × 1800s = 45 J

## 3. Estimativa do Consumo Total de Energia

Considerando a eficiência do regulador de tensão (Buck Converter) estimada em 90% para os componentes de 5V e o consumo direto dos motores em 7.4V, o consumo total de energia do sistema por sessão é de aproximadamente **5564 Joules**.

## 4. Escolha da Fonte de Alimentação

**Conversão para Watt-hora:** 5564 J ÷ 3600 ≈ 1.55 Wh.

**Margem de Segurança:** Para sistemas robóticos móveis, é usual adotar uma margem de segurança de 20% a 50% para compensar perdas internas das baterias, variações de temperatura e resistência interna. No entanto, para garantir estabilidade absoluta contra picos de corrente superiores a 1000mA nas curvas, o grupo adotou uma margem severa de segurança.

**Seleção da Bateria:** Foi selecionada a bateria **Li-ion 18650 7.4V de 2000mAh**. Esta bateria oferece aproximadamente 14.8 Wh, suprindo a demanda teórica com uma folga de cerca de 850%, o que elimina riscos de brownouts durante a operação dos motores.

## 5. Planejamento do Circuito de Alimentação

O planejamento do circuito foca na estabilidade da tensão para a lógica e proteção dos componentes.

* **Regulação:** Uso do conversor buck MP1584 para reduzir a tensão da bateria de 7.4V para 5V estáveis, garantindo alta eficiência energética e baixa dissipação térmica.
* **Controle de Oscilações:** Inclusão de capacitores de filtragem (100nF e 10µF) na entrada e saída do conversor, além de capacitores de desacoplamento próximos ao driver de motor e sensores para mitigar ruídos de back-EMF.
* **Proteções:** Implementação de chave geral física e isolamento para os motores via driver TB6612FNG. Recomenda-se a inserção de fusível ou PTC na linha principal para proteção contra sobrecorrente.

## 6. Monitoramento via Software

No firmware do ESP32, será implementada uma rotina de monitoramento através de um divisor resistivo conectado a um pino ADC para medir a tensão da bateria em tempo real. Os dados de tempo de operação e níveis de tensão serão registrados e transmitidos via telemetria para análise posterior da curva de descarga.

## 7. Validação com Testes Reais

Testes de campo serão realizados para validar os cálculos teóricos. Espera-se que o consumo real apresente variações devido a:

* Atrito mecânico irregular no labirinto.
* Perdas por efeito Joule nos condutores e conectores.
* Eficiência real dos motores em diferentes níveis de carga (PWM).

A literatura sugere que diferenças de até 15% entre o teórico e o real são comuns em sistemas embarcados de pequeno porte devido a variáveis ambientais e tolerâncias de componentes. Os dados coletados pelo software de monitoramento servirão para refinar o modelo de consumo e ajustar, se necessário, as estratégias de gerenciamento de energia.