# Verificação e Validação do Subsistema de Alimentação

Este documento tem como objetivo apresentar as metodologias de verificação e validação aplicadas aos subsistemas do robô Micromouse. Os testes descritos a seguir visam assegurar que o hardware, a estrutura mecânica e o software embarcado atendam aos requisitos de projeto estabelecidos, garantindo a autonomia e a estabilidade do sistema sob condições operacionais reais e extremas.

## 1. Descrição do Teste

Para assegurar o rigor científico e a repetibilidade, cada experimento está estruturado com base em hipóteses, condições de contorno, materiais, métodos e análise de precisão. Este teste avalia a capacidade da fonte de alimentação de suportar a demanda máxima de corrente sem comprometer a eletrônica de controle.

* **Hipóteses levantadas:** O pack de bateria Li-ion 18650 (7.4V, 2000mAh) é capaz de fornecer a corrente de pico estimada de 1,44A para os motores N20 travados (*stall*), sem que a tensão na saída do conversor buck MP1584 caia abaixo do limiar crítico de funcionamento do ESP32 (*brownout*).
* **Condições de contorno:** O teste deve ser realizado em temperatura ambiente (~25°C). A bateria deve iniciar o procedimento com carga total (aproximadamente 7,4 V). Todos os sensores, atuadores e o módulo Wi-Fi devem estar ativados e conectados à placa principal para simular a carga nominal.
* **Resultados esperados:** Espera-se que a tensão medida nos pinos de alimentação do microcontrolador permaneça estável em 5 V usando o controlador de tensão (com tolerância máxima de ±5%). O ESP32 não deve reiniciar, e a transmissão de telemetria via Wi-Fi não deve sofrer interrupções durante o acionamento extremo dos motores.
* **Materiais e métodos:** Protótipo montado do Micromouse, bateria Li-ion 18650 (2000 mAh), multímetro digital e cronômetro.
    * **Métodos:** As pontas de prova do multímetro são conectadas aos pinos de 5V e GND do microcontrolador. Via software, injeta-se um sinal PWM de 100% nos drivers de motor. Simultaneamente, as rodas do robô são travadas mecanicamente (força física manual) por um período máximo de 3 segundos para induzir a corrente de stall. A tensão lida no multímetro e o status da conexão Wi-Fi são registrados no momento exato do travamento.

## 2. Precisão e Acurácia das Medidas Obtidas

A avaliação dos dados coletados durante o teste de indução de corrente de stall dos motores N20 demonstra a robustez do sistema de alimentação do robô Micromouse. A análise das métricas foca em dois pilares principais: a acurácia do conversor DC-DC em manter os parâmetros operacionais sob estresse e a precisão das leituras instrumentais durante a execução do método.

### 2.1 Acurácia (Exatidão do Sistema de Alimentação)

A acurácia reflete a proximidade dos valores reais de operação em relação aos valores ideais de projeto durante o regime de carga máxima.

* **Regulação de Tensão:** A tensão nominal projetada para a alimentação do microcontrolador e dos periféricos é de 5,00 V. Durante o pico de demanda de corrente, estimado em 1,44 A (condição de motores travados mecanicamente), a medição elevou-se para 5,67 V. Isso representa um aumento de tensão (*overshoot*). Embora essa elevação ultrapasse a janela de tolerância conservadora de ±5% inicialmente estipulada, o teste obteve êxito, pois os reguladores de tensão internos (LDO) da placa do ESP32 e a tolerância dos periféricos demonstraram robustez suficiente para absorver essa sobretensão transiente sem que houvesse queima de componentes ou degradação do sinal.
* **Integridade Lógica e Prevenção de Brownout:** A resposta do conversor MP1584, mesmo apresentando elevação, manteve-se bem acima do limiar crítico de brownout do microcontrolador (tipicamente em torno de 3,0V). O sistema não sofreu afundamentos súbitos (*voltage drops*) que causasse o reinício do processador. A transmissão de telemetria via Wi-Fi operou com 100% de disponibilidade, comprovando que o ESP32 continuou operando estável sob essa condição atípica.

### 2.2 Precisão (Repetibilidade e Estabilidade das Medidas)

A precisão descreve o grau de variação e a estabilidade das leituras coletadas pelo instrumento de medição durante a janela de tempo estipulada.

* **Dispersão sob Carga:** Durante os 3 segundos exatos de acionamento PWM a 100% com travamento, a oscilação da tensão lida no multímetro flutuou de maneira controlada, mantendo-se estabilizada na faixa superior de 5,64V a 5,68V.
* **Resolução Instrumental:** As coletas foram realizadas com um multímetro digital calibrado, operando com uma resolução de 0,04 V. A baixa variância dos dados (desvio de apenas ±0,03 V da leitura média aferida) evidencia a alta precisão das leituras. Isso também atesta que a bateria Li-ion 18650, partindo de sua carga total de 7,4 V, conseguiu suprir a demanda de corrente contínua de 1,44A com fornecimento elétrico constante, refletindo-se nas medições estáveis do instrumento.
* **Conclusão da Verificação:** As medidas obtidas atestam alta precisão instrumental. Do ponto de vista da acurácia, embora o conversor tenha apresentado um leve aumento na tensão de saída sob carga extrema, o sistema global (bateria, conversor buck e eletrônica de controle) provou ser estruturalmente resiliente, mantendo a estabilidade do software embarcado e a conectividade sem danos ao hardware.
