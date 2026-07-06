# Análise de Desempenho e Benchmark: Subsistema de Energia

## 1. Objetivo
Este documento apresenta o benchmark técnico e a justificativa para a homologação do pack de bateria **Li-ion 18650 (2S)** como fonte principal de alimentação do protótipo Micromouse, avaliando sua capacidade de suprir os componentes lógicos e de potência do robô sob condições nominais e de estresse.

## 2. Especificações Nominais da Bateria Escolhida
O pack homologado é composto por duas células cilíndricas no formato 18650, utilizando a química de Íons de Lítio (Li-ion), configuradas em série (2S).

* **Tensão Nominal do Pack:** 7.4V
* **Tensão de Carga Completa:** 8.4V (4.2V por célula)
* **Tensão Mínima de Corte (Cut-off):** 6.0V (3.0V por célula)
* **Capacidade Nominal:** 2000 mAh
* **Peso Total Estimado:** ~100 gramas

## 3. Benchmark de Desempenho no Sistema

### 3.1. Corrente e Capacidade de Torque
* **Exigência de Pico do Robô (Motores em Stall + Wi-Fi):** ~1,44 A
* **Veredito:** **Aprovado.** O pack suporta com extrema folga o pior cenário de pico energético do robô. Essa margem garante que não ocorrerá queda abrupta de tensão (*voltage sag*) capaz de provocar o reinício do microcontrolador (ESP32) durante arranques rápidos ou colisões contra as paredes do labirinto.

### 3.2. Autonomia (Tempo de Pista)
* **Exigência do Projeto:** 30 minutos (0,5 horas) de operação contínua.
* **Consumo Nominal Estimado do Robô:** ~505 mA.
* **Autonomia Projetada (2000mAh / 505mA):** ~3,96 horas (Aprox. 237 minutos).
* **Veredito:** **Aprovado.** A capacidade de 2000mAh oferece uma margem de segurança operacional altíssima, permitindo múltiplas sessões de testes contínuos de software sem necessidade de recarga.

### 3.3. Comportamento Dinâmico (Relação Peso/Potência)
* **Peso da Bateria:** ~100g.
* **Veredito:** **Trade-off Aceito.** Embora o peso seja superior ao de packs menores, a equipe priorizou a estabilidade elétrica absoluta. O chassi e a distribuição de peso deverão ser ajustados para acomodar os 100g centralizados, minimizando o impacto na inércia dos motores N20.

## 4. Análise Energética e Proteções Adicionais

### 4.1 Reguladores de Tensão
Considerando o uso de uma bateria Li-ion 2S (7,4V nominal), observa-se que a tensão de alimentação varia aproximadamente entre 8,4V (carga total) e 6,0V (descarga). Essa variação inviabiliza a alimentação direta dos componentes eletrônicos, especialmente o microcontrolador ESP32, que opera em 3,3V. Foi adotado o uso de um conversor buck baseado no MP1584, responsável por reduzir a tensão da bateria para 5V. Essa escolha se justifica por sua alta eficiência energética, capacidade de fornecer corrente suficiente para picos dos motores e menor dissipação térmica.

Para controle das oscilações geradas pela variação dinâmica da carga, foram adotadas as seguintes estratégias:
* Uso de capacitores de filtragem na entrada e saída do conversor buck.
* Capacitores de desacoplamento distribuídos no circuito.

### 4.2 Proteção contra Sobrecorrente e Ruídos
A análise do consumo indica picos de até aproximadamente 1470 mA, principalmente devido à corrente de partida dos motores DC. Sem mecanismos de proteção, esses picos podem causar queda brusca de tensão (brownout), reset do microcontrolador ou danos à bateria e trilhas do circuito.

Para mitigar esses riscos, são recomendadas as seguintes medidas:
* Inserção de fusível ou PTC na linha principal de alimentação.
* Dimensionamento adequado das trilhas e condutores.

Os motores DC geram ruídos elétricos e picos de tensão (back-EMF), que podem interferir diretamente no funcionamento do sistema lógico. Para reduzir esses efeitos, o projeto incorpora capacitores de desacoplamento (100nF e 10µF) próximos aos sensores e drivers, separação das linhas de alimentação (lógica e potência) e uso de diodos de proteção (flyback) quando aplicável.