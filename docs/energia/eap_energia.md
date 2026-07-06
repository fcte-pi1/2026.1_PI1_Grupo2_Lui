# Estrutura Analítica do Projeto (EAP) - Subsistema de Energia

## 1. Visão Geral da Energia
O subsistema de energia do Micromouse tem como objetivo fornecer alimentação elétrica contínua e estável para todos os componentes do robô (microcontrolador, atuadores e sensores), garantindo a autonomia necessária para a conclusão dos três labirintos sem quedas de tensão (brownouts) ou necessidade de recarga durante a prova. A solução é composta por quatro partes principais:

| COMPONENTE | TECNOLOGIA / PEÇA | RESPONSABILIDADE |
| :--- | :--- | :--- |
| **Fonte de Alimentação** | Bateria Li-ion 18650 (2S, 7.4V, 2000mAh) | Fornecer a carga bruta e a corrente de pico necessária para o funcionamento autônomo do robô. |
| **Regulação Lógica** | Buck Converter MP1584 | Reduzir e estabilizar a tensão de 7.4V para 5V (alimentação do ESP32 e lógica dos sensores). |
| **Regulação de Potência** | Driver TB6612FNG (Ponte H) | Distribuir a tensão da bateria de forma controlada (via PWM) para os motores N20. |
| **Proteção e Filtragem** | Chave ON/OFF, Capacitores (100nF, 10µF) | Ligar/desligar o circuito, prevenir curtos e filtrar ruídos eletromagnéticos gerados pelas escovas dos motores. |

**O fluxo principal do sistema é:**
`Bateria (7.4V) ➔ Chave ON/OFF ➔ [Buck Converter (5V) ➔ ESP32/Sensores] E [Ponte H ➔ Motores]`

## 2. Estrutura do Projeto de Energia
**Objetivo da EAP:** A EAP organiza o trabalho de hardware e eletrônica do projeto Micromouse em pacotes menores, facilitando o dimensionamento, a aquisição de peças, a divisão das soldagens e a validação de segurança antes da integração com o software.

* **Produto:** Circuito de alimentação montado, soldado, isolado e fixado no chassi junto à bateria.
* **Serviço:** Fornecimento contínuo, seguro e estável de tensão e corrente elétrica durante a corrida.
* **Projeto:** Esforço temporário da equipe de hardware para dimensionar, comprar, esquematizar, soldar, testar e documentar o subsistema de energia.

## 3. EAP Resumida

**2. Energia do Micromouse**
* **2.1 Gestão e Documentação**
  * Levantamento de requisitos de tensão e corrente
  * Dimensionamento energético e autonomia
  * Orçamento e lista de compras (BOM)
  * Registro de decisões técnicas (Adoção da bateria de 2000mAh)
* **2.2 Aquisição de Componentes**
  * Compra da bateria 18650 e carregador
  * Compra dos reguladores, conectores e capacitores
  * Recebimento e conferência do hardware
* **2.3 Projeto do Hardware**
  * Diagrama de blocos de energia
  * Esquemático das ligações de potência e lógica
  * Planejamento de roteamento (posicionamento na placa)
* **2.4 Montagem e Soldagem**
  * Preparação dos conectores da bateria
  * Soldagem do circuito Buck Converter
  * Adição dos capacitores de desacoplamento
  * Soldagem da Chave ON/OFF
* **2.5 Testes e Validação**
  * Teste de continuidade (prevenção de curto-circuito)
  * Aferição da tensão de saída do regulador (5V) sem carga
  * Teste de estresse com motores em pico (Stall)
  * Validação da autonomia em bancada (30 min)
* **2.6 Integração e Entrega**
  * Fixação da bateria de 100g no chassi (centro de gravidade)
  * Conexão do módulo de energia à placa lógica principal

## 4. Dicionário Resumido da EAP
* **2.1 Gestão e Documentação:** Memorial de cálculo aprovado. Critério: Dimensionamento (mAh) e correntes de pico documentados e revisados.
* **2.2 Aquisição de Componentes:** Peças em mãos. Critério: Bateria, regulador, capacitores e conectores entregues e testados individualmente.
* **2.3 Projeto do Hardware:** Esquemático eletrônico. Critério: Diagrama finalizado ilustrando ligações, polaridades e componentes.
* **2.4 Montagem e Soldagem:** Circuito de força montado. Critério: Placa soldada de forma limpa, sem soldas frias e com isolamento adequado (espaguete termo-retrátil).
* **2.5 Testes e Validação:** Qualidade e segurança verificadas. Critério: Regulador fornecendo exatos 5V; ausência de curtos; ESP32 não reiniciando no pico do motor.
* **2.6 Integração e Entrega:** Energia integrada ao robô. Critério: Bateria fixa no chassi, alimentando sistema de forma autônoma (sem cabos externos).

## 5. Riscos Principais
* **Atraso na entrega da bateria:** Comprar via Mercado Livre com envio rápido ("Full") ou lojas físicas de modelismo em Brasília.
* **Curto-circuito durante montagem:** Usar multímetro (teste de continuidade) antes de plugar a bateria; isolar expostos termicamente.
* **Peso excessivo:** Ajustar o chassi para centralizar as 100g e testar o impacto na agilidade dos motores N20.
* **Inversão de polaridade:** Utilizar conectores polarizados (ex: JST, XT30) e padronizar cores de fios.
* **Bateria descarregar até estragar:** Criar uma Issue para a equipe de Software monitorar a tensão via divisor resistivo no ESP32.

## 6. Rastreabilidade
* **Estabilidade Lógica (ESP32 5V):** Pacotes 2.3, 2.4, 2.5 | CT-EN-01: Medição de saída do Buck sem carga e com carga.
* **Autonomia de Corrida (30 min):** Pacotes 2.1, 2.5 | CT-EN-02: Teste de bancada contínuo com Wi-Fi e motores ligados.
* **Supressão de Ruídos:** Pacotes 2.3, 2.4 | CT-EN-03: Validação da leitura do sensor ToF enquanto motores aceleram.
* **Segurança de Circuito:** Pacotes 2.4, 2.5 | CT-EN-04: Teste de continuidade de trilhas e atuação da chave geral.