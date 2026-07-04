# Roteiro de Testes de Software e Qualidade

## Introdução

A matriz de testes funcionais consolida o roteiro de validação do software do Micromouse, abrangendo as camadas de firmware, simulador, backend, banco de dados e dashboard web. Cada caso é estruturado segundo um conjunto fixo de atributos (código identificador, objetivo, requisitos cobertos, técnica aplicada, partições e fronteiras, casos concretos, pré-condições, procedimentos, resultado esperado, reparo e pós-reparo), o que viabiliza a rastreabilidade entre requisitos e evidências de validação, assegurando, ao mesmo tempo, a reprodutibilidade dos testes ao longo do desenvolvimento.

A seleção dos casos fundamenta-se nas técnicas de partição de equivalência e análise de valores-limite, conforme discutidas por Aniche (2022). A partir desses critérios, foram identificados cenários representativos de entradas válidas, entradas inválidas e pontos críticos de mudança de comportamento, tais como limites de detecção, tempos de resposta, tamanho de pacotes e transições de estado de navegação. O escopo restringe-se ao comportamento do software, tratando dimensões físicas do Tema PI1 apenas quando estas afetam diretamente sua representação lógica no sistema.

## Como Ler os Testes

Cada caso é estruturado a partir do requisito que pretende validar. As partições de equivalência consolidam entradas ou estados que devem produzir comportamento semelhante, como parede presente, parede ausente, pacote válido ou pacote inválido. Os valores-limite exercitam os pontos em que o comportamento do software pode alterar-se, como 5 cm para detecção de parede, 500 ms para latência ou 512 bytes para tamanho de pacote. A seleção dos casos concretos combina valores típicos, valores próximos às fronteiras e cenários inválidos, ampliando a capacidade de revelar falhas sem tornar o roteiro excessivamente extenso.

| Atributo | Descrição |
|---|---|
| **Objetivo** | O comportamento de software que será verificado. |
| **Requisito(s) coberto(s)** | RF, US, RNF, RE ou item do Tema PI1 relacionado. |
| **Técnica de teste** | Estratégia usada para escolher os casos, como partição de equivalência, análise de fronteira, teste negativo ou transição de estado. |
| **Partições e fronteiras** | Classes de entrada e limites onde há maior chance de erro. |
| **Casos concretos** | Entradas mínimas que representam as partições. |
| **Pré-condições** | Estado necessário antes de executar o teste. |
| **Procedimentos** | Passos práticos para executar o teste. |
| **Resultado Esperado** | Asserção que decide se o teste passou. |
| **Reparo** | Ação provável caso o teste falhe. |
| **Pós-Reparo** | O que deve ser reexecutado após a correção. |

---

## Firmware e Simulador

Os testes de firmware e simulador delimitam a verificação das unidades de software embarcado responsáveis pela interpretação dos sensores, pelo acionamento dos motores, pelo processamento da odometria e pela inicialização dos componentes lógicos do Micromouse. Esta etapa antecipa falhas de baixo nível previamente à integração com a navegação e com os serviços web, viabilizando diagnósticos mais precisos.

### CT-01 — Detecção Frontal

| Atributo | Descrição |
|---|---|
| **Objetivo** | Diferenciar parede frontal próxima de ausência de parede usando o limite configurado no firmware. |
| **Requisito(s) coberto(s)** | RF-02 / US02; RE-03 |
| **Técnica de teste** | Partição de equivalência + análise de fronteira |
| **Partições e fronteiras** | Parede presente, valor de fronteira e parede ausente. Limite de referência: 5 cm. |
| **Casos concretos** | 3 cm, 4,9 cm, 5,0 cm, 5,1 cm e 10 cm. |
| **Pré-condições** | Firmware carregado ou simulador de sensores ativo; log/Serial Monitor habilitado. |
| **Procedimentos** | 1. Injetar leitura frontal de 3 cm.<br>2. Registrar o estado calculado pelo firmware.<br>3. Repetir para 4,9 cm, 5,0 cm, 5,1 cm e 10 cm.<br>4. Comparar o estado final com a regra de detecção definida no código. |
| **Resultado Esperado** | Leituras abaixo do limite são classificadas como parede; leituras acima são classificadas como ausência; o valor de 5,0 cm segue exatamente a regra definida no firmware. |
| **Reparo** | Ajustar constante de referência ou comparação usada na detecção frontal. |
| **Pós-Reparo** | Reexecutar os cinco valores de teste e confirmar a mudança de estado somente na fronteira esperada. |

### CT-02 — Detecção Lateral

| Atributo | Descrição |
|---|---|
| **Objetivo** | Garantir que sensores laterais esquerdo e direito sejam interpretados de forma independente. |
| **Requisito(s) coberto(s)** | RF-02 / US02; RE-03 |
| **Técnica de teste** | Partição de equivalência + teste de independência entre entradas |
| **Partições e fronteiras** | Esquerda ativa, direita ativa, ambas livres e ambas bloqueadas. |
| **Casos concretos** | `(E=parede,D=livre)`, `(E=livre,D=parede)`, `(E=livre,D=livre)`, `(E=parede,D=parede)`. |
| **Pré-condições** | Rotina de leitura lateral acessível por teste unitário, simulador ou log do firmware. |
| **Procedimentos** | 1. Injetar parede apenas no lado esquerdo.<br>2. Registrar os estados esquerdo e direito.<br>3. Repetir com parede apenas no lado direito.<br>4. Repetir com ambos livres e ambos bloqueados. |
| **Resultado Esperado** | A alteração de um lado não muda indevidamente o estado do outro lado. |
| **Reparo** | Corrigir mapeamento de pinos, índices ou variáveis usadas nas leituras laterais. |
| **Pós-Reparo** | Reexecutar as quatro combinações e confirmar independência das leituras. |

### CT-03 — Sentido dos Motores

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que os comandos lógicos de frente e ré produzem o sentido de rotação esperado para cada motor. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-07 / US07 |
| **Técnica de teste** | Tabela de decisão por comando |
| **Partições e fronteiras** | Motor esquerdo, motor direito, comando de frente e comando de ré. |
| **Casos concretos** | `frente` e `ré` aplicados aos motores esquerdo e direito. |
| **Pré-condições** | Driver de motores acessível por log, mock de pinos ou teste de firmware. |
| **Procedimentos** | 1. Enviar comando `frente` para os motores esquerdo e direito.<br>2. Registrar as saídas lógicas do driver.<br>3. Enviar comando `ré` para os motores esquerdo e direito.<br>4. Comparar as saídas com a tabela esperada. |
| **Resultado Esperado** | Cada comando aciona direção compatível com o movimento esperado, sem inversão indevida entre os motores. |
| **Reparo** | Corrigir tabela de comando, inversão lógica ou mapeamento de pinos do driver. |
| **Pós-Reparo** | Reexecutar frente e ré para ambos os motores. |

### CT-04 — Níveis de Potência

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a conversão de potência percentual para PWM é limitada e monotônica. |
| **Requisito(s) coberto(s)** | RF-07 / US07 |
| **Técnica de teste** | Partição de equivalência por faixa + análise de fronteira |
| **Partições e fronteiras** | Potência mínima, baixa, média, máxima; fronteiras 0% e 100%. |
| **Casos concretos** | 0%, 30%, 60% e 100%. |
| **Pré-condições** | Função de conversão para PWM acessível por teste unitário ou log do firmware. |
| **Procedimentos** | 1. Executar conversão para 0%.<br>2. Executar conversão para 30%, 60% e 100%.<br>3. Registrar o PWM calculado em cada caso.<br>4. Comparar ordem e limites. |
| **Resultado Esperado** | O PWM fica dentro da faixa válida e cresce de forma monotônica conforme a potência aumenta. |
| **Reparo** | Ajustar fórmula de conversão, saturação ou escala de PWM. |
| **Pós-Reparo** | Reexecutar os quatro percentuais e verificar limites novamente. |

### CT-05 — Leitura de Encoder

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar a contagem de pulsos do encoder em uma volta completa. |
| **Requisito(s) coberto(s)** | RF-01 / US01 |
| **Técnica de teste** | Análise de fronteira + repetição para estabilidade |
| **Partições e fronteiras** | Sem movimento, uma volta completa e múltiplas voltas; tolerância de pulso. |
| **Casos concretos** | 0 volta, 1 volta e 3 voltas; variação máxima de +/-1 pulso por volta. |
| **Pré-condições** | Contador de encoder acessível por teste unitário, simulador ou log/Serial Monitor. |
| **Procedimentos** | 1. Zerar o contador.<br>2. Simular ausência de movimento e registrar a contagem.<br>3. Simular uma volta completa e registrar a contagem de pulsos.<br>4. Simular três voltas consecutivas e registrar a variação. |
| **Resultado Esperado** | A contagem corresponde ao valor especificado, com variação máxima de +/-1 pulso por volta. |
| **Reparo** | Corrigir pino de leitura, lógica de interrupção ou constante de pulsos por volta. |
| **Pós-Reparo** | Confirmar contagem consistente em três voltas consecutivas. |

### CT-06 — Inicialização Geral

| Atributo | Descrição |
|---|---|
| **Objetivo** | Assegurar que o firmware inicializa componentes e entra em estado pronto/ocioso. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-02 / US02; RF-08 / US08; RE-01; RE-03 |
| **Técnica de teste** | Teste de transição de estado |
| **Partições e fronteiras** | Inicialização normal e reinicialização. |
| **Casos concretos** | Primeiro boot e segundo boot consecutivo. |
| **Pré-condições** | Firmware com logs de inicialização habilitados ou ambiente de simulação equivalente. |
| **Procedimentos** | 1. Iniciar o firmware.<br>2. Monitorar a sequência de inicialização no log/Serial Monitor.<br>3. Aguardar o estado de ociosidade.<br>4. Repetir a inicialização para confirmar estabilidade. |
| **Resultado Esperado** | A sequência de boot ocorre sem erro e o estado final permanece ocioso/pronto para uso. |
| **Reparo** | Corrigir ordem de inicialização ou configuração dos componentes lógicos. |
| **Pós-Reparo** | Confirmar sequência de boot estável em duas tentativas. |

---

## Navegação e Mapeamento

Os testes de navegação e mapeamento consolidam a validação das rotinas que convertem leituras sensoriais em decisões de movimento, atualização de mapa e cálculo de rota. A organização dos casos permite compreender o comportamento do firmware diante de corredores, curvas, becos sem saída, fronteiras de célula e transições entre as fases de exploração e corrida otimizada.

### CT-07 — Parada de Segurança

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a máquina de estados interrompe movimento quando há obstáculo frontal no limite de segurança. |
| **Requisito(s) coberto(s)** | RF-02 / US02; RF-07 / US07 |
| **Técnica de teste** | Análise de fronteira + transição de estado |
| **Partições e fronteiras** | Caminho livre, obstáculo próximo, valor limite de segurança (ex: 5 cm) e bloqueio. |
| **Casos concretos** | 30 cm (livre), 10 cm (próximo), 5,1 cm (limite superior), 5,0 cm (valor limite exato) e 4,9 cm (limite inferior/bloqueio). |
| **Pré-condições** | Simulador de navegação ou firmware com injeção de leitura frontal. |
| **Procedimentos** | 1. Colocar o estado inicial como `movendo`.<br>2. Injetar cada distância frontal.<br>3. Registrar a transição de estado.<br>4. Comparar com a regra de parada. |
| **Resultado Esperado** | O estado muda de `movendo` para `parado` quando o limite de segurança é atingido. |
| **Reparo** | Ajustar threshold de parada ou condição da máquina de estados. |
| **Pós-Reparo** | Reexecutar os valores ao redor do limite. |

### CT-08 — Avanço de Célula

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar o deslocamento lógico de uma célula a partir da integração entre comando de motor e encoder. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-07 / US07 |
| **Técnica de teste** | Análise de fronteira de deslocamento |
| **Partições e fronteiras** | Ausência de deslocamento, avanço de uma célula e repetição de avanços. |
| **Casos concretos** | 0 célula, 1 célula de 18 cm e 5 avanços consecutivos de 1 célula. |
| **Pré-condições** | Conversão célula-centímetro disponível; geometria lógica configurada com célula de 18 cm. |
| **Procedimentos** | 1. Definir posição inicial.<br>2. Executar comando de avanço de 1 célula.<br>3. Registrar a posição lógica de parada.<br>4. Repetir cinco avanços consecutivos.<br>5. Conferir posição lógica e distância-alvo. |
| **Resultado Esperado** | Cada comando desloca exatamente uma célula lógica, mantendo coerência com a distância de 18 cm por célula. |
| **Reparo** | Corrigir conversão célula-centímetro ou atualização de coordenadas. |
| **Pós-Reparo** | Reexecutar cinco avanços consecutivos de uma célula. |

### CT-09 — Trajetória Contínua

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que sequências de avanço acumulam posição sem deriva lógica. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-07 / US07 |
| **Técnica de teste** | Teste de sequência |
| **Partições e fronteiras** | Sequência curta, sequência longa e corredor livre. |
| **Casos concretos** | Avançar 3 células em um corredor lógico de 4 células. |
| **Pré-condições** | Simulador com corredor livre e posição inicial conhecida. |
| **Procedimentos** | 1. Posicionar robô simulado no início do corredor.<br>2. Executar três comandos consecutivos de avanço.<br>3. Registrar posição e orientação final.<br>4. Verificar colisão lógica ou desvio de célula. |
| **Resultado Esperado** | O estado final indica a terceira célula correta, orientação preservada e ausência de colisão simulada. |
| **Reparo** | Corrigir acúmulo de posição, orientação ou atualização de célula. |
| **Pós-Reparo** | Reexecutar a sequência de três avanços. |

### CT-10 — Correção de Rumo

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o controle gera correção proporcional ao desalinhamento lateral. |
| **Requisito(s) coberto(s)** | RF-07 / US07; RNF-04 |
| **Técnica de teste** | Partição por sinal e magnitude do erro |
| **Partições e fronteiras** | Desalinhamento à esquerda, alinhamento central e desalinhamento à direita. |
| **Casos concretos** | -10°, 0° e 10°. |
| **Pré-condições** | Função de controle acessível por teste unitário ou simulador. |
| **Procedimentos** | 1. Injetar desalinhamento lateral de aproximadamente -10° e registrar correção.<br>2. Repetir para 0° e 10°.<br>3. Comparar sinal e magnitude da saída. |
| **Resultado Esperado** | A saída do controlador corrige o desalinhamento para o eixo central e tende a zero quando o erro é zero. |
| **Reparo** | Ajustar ganho, sinal da correção ou normalização do erro. |
| **Pós-Reparo** | Reexecutar os três ângulos. |

### CT-11 — Curvas de 90°

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que curvas de 90° atualizam a orientação lógica corretamente. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-07 / US07 |
| **Técnica de teste** | Tabela de decisão por direção de curva |
| **Partições e fronteiras** | Curva esquerda, curva direita e sequência alternada. |
| **Casos concretos** | `N->O`, `N->L` e sequência esquerda-direita. |
| **Pré-condições** | Máquina de estados de orientação acessível por teste unitário ou simulador. |
| **Procedimentos** | 1. Definir orientação inicial como Norte.<br>2. Executar curva à esquerda.<br>3. Reiniciar orientação e executar curva à direita.<br>4. Executar sequência esquerda-direita.<br>5. Registrar orientação final. |
| **Resultado Esperado** | A orientação final corresponde à direção calculada para cada curva. |
| **Reparo** | Corrigir tabela de orientação ou atualização angular. |
| **Pós-Reparo** | Reexecutar esquerda, direita e sequência alternada. |

### CT-12 — Meia-Volta (180°)

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o firmware escolhe meia-volta quando frente, esquerda e direita estão bloqueadas. |
| **Requisito(s) coberto(s)** | RF-02 / US02; RF-03 / US03; RF-07 / US07 |
| **Técnica de teste** | Teste de transição de estado |
| **Partições e fronteiras** | Beco sem saída, corredor aberto e curva possível. |
| **Casos concretos** | Paredes em frente, esquerda e direita; caminho livre atrás. |
| **Pré-condições** | Simulador de sensores e decisão de navegação habilitado. |
| **Procedimentos** | 1. Configurar paredes em F/E/D.<br>2. Executar decisão de próximo movimento.<br>3. Registrar ação escolhida e atualização do mapa. |
| **Resultado Esperado** | O bloqueio é identificado e a próxima ação é uma rotação de 180° para saída do beco sem colisão lógica. |
| **Reparo** | Corrigir condição booleana de bloqueio ou prioridade de decisão. |
| **Pós-Reparo** | Reexecutar cenário de beco e um cenário de curva possível. |

### CT-13 — Escrita de Mapa

| Atributo | Descrição |
|---|---|
| **Objetivo** | Garantir que paredes detectadas sejam gravadas na matriz interna na coordenada correta. |
| **Requisito(s) coberto(s)** | RF-01 / US01; RF-03 / US03 |
| **Técnica de teste** | Partição de equivalência por configuração de paredes |
| **Partições e fronteiras** | Sem parede, uma parede, laterais e múltiplas paredes. |
| **Casos concretos** | Livre; frente; esquerda; direita; frente+esquerda. |
| **Pré-condições** | Mapa interno inicializado e função de escrita acessível por teste. |
| **Procedimentos** | 1. Definir coordenada atual.<br>2. Injetar cada configuração de paredes.<br>3. Imprimir ou consultar matriz interna.<br>4. Comparar bits/valores gravados com a entrada. |
| **Resultado Esperado** | A matriz contém exatamente as paredes informadas na célula correta. |
| **Reparo** | Corrigir índices X/Y, orientação relativa ou manipulação de bits. |
| **Pós-Reparo** | Reexecutar as cinco configurações em mais de uma coordenada. |

### CT-14 — Lógica Flood Fill

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o algoritmo calcula pesos e escolhe o vizinho de menor custo rumo ao centro. |
| **Requisito(s) coberto(s)** | RF-03 / US03; RF-04 / US04; RF-05 / US05 |
| **Técnica de teste** | Teste baseado em especificação com mapa fixture |
| **Partições e fronteiras** | Caminho livre, célula bloqueada, beco e rota alternativa. |
| **Casos concretos** | Labirinto fixture com caminho esperado e obstáculos conhecidos. |
| **Pré-condições** | CT-13 aprovado; mapa fixture carregado no simulador ou teste unitário. |
| **Procedimentos** | 1. Carregar mapa conhecido.<br>2. Executar Flood Fill.<br>3. Registrar pesos calculados.<br>4. Registrar próximo movimento escolhido.<br>5. Comparar com o resultado esperado do fixture. |
| **Resultado Esperado** | Pesos e próximo movimento coincidem com o resultado esperado do mapa fixture. |
| **Reparo** | Corrigir fila, cálculo de vizinhos, tratamento de paredes ou definição do centro. |
| **Pós-Reparo** | Reexecutar o mesmo mapa e um mapa com rota alternativa. |

### CT-15 — Fase 1: Exploração

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar exploração autônoma em mapa desconhecido simulado até alcançar o objetivo. |
| **Requisito(s) coberto(s)** | RF-01 a RF-07; RNF-04 |
| **Técnica de teste** | Teste de sistema em simulador |
| **Partições e fronteiras** | Corredor, bifurcação, beco e centro. |
| **Casos concretos** | Labirinto fixture com destino conhecido e paredes inicialmente desconhecidas para o robô. |
| **Pré-condições** | CT-08 a CT-14 aprovados; simulador de labirinto disponível. |
| **Procedimentos** | 1. Carregar labirinto desconhecido para o firmware.<br>2. Iniciar modo exploração.<br>3. Registrar decisões, paredes descobertas e recálculos.<br>4. Acompanhar até a chegada ao centro. |
| **Resultado Esperado** | O robô simulado atualiza o mapa, recalcula rotas e alcança o centro sem intervenção externa. |
| **Reparo** | Ajustar máquina de estados, regra de exploração ou integração com Flood Fill. |
| **Pós-Reparo** | Reexecutar a exploração no mesmo fixture. |

### CT-16 — Fase 2: Corrida

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o robô usa mapa aprendido para seguir o menor caminho conhecido sem reexplorar becos. |
| **Requisito(s) coberto(s)** | RF-03 / US03; RF-04 / US04; RF-05 / US05; RF-06 / US06 |
| **Técnica de teste** | Teste de sistema com estado persistido |
| **Partições e fronteiras** | Mapa completo, becos conhecidos e rota direta. |
| **Casos concretos** | Reexecutar o fixture do CT-15 com mapa já persistido. |
| **Pré-condições** | CT-15 aprovado; mapa completo disponível na memória lógica. |
| **Procedimentos** | 1. Restaurar mapa aprendido.<br>2. Reiniciar posição lógica na largada.<br>3. Iniciar modo corrida.<br>4. Registrar sequência de células percorridas. |
| **Resultado Esperado** | A sequência de células corresponde ao menor caminho esperado e evita becos já conhecidos. |
| **Reparo** | Corrigir leitura do mapa persistido, custo de rota ou seleção de próximo movimento. |
| **Pós-Reparo** | Reexecutar corrida otimizada no mesmo mapa. |

---

## Backend e Persistência

Os testes de backend e persistência validam a camada responsável pela recepção da telemetria, pela validação dos payloads, pela retransmissão dos eventos ao dashboard e pela consolidação dos dados finais no banco. A matriz delimita casos de fluxo válido, rejeição de entradas inválidas, ordenação temporal do processamento e consulta histórica, assegurando que a camada de servidor preserve a consistência e a rastreabilidade dos dados.

### CT-17 — Recepção de Telemetria

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o backend aceita payload válido de telemetria e valida os campos obrigatórios. |
| **Requisito(s) coberto(s)** | RF-08 / US08; RF-09 / US09; RNF-09 |
| **Técnica de teste** | Partição de equivalência de payload válido |
| **Partições e fronteiras** | Payload mínimo válido, completo típico e completo 16x16. |
| **Casos concretos** | Posição, paredes, bateria, velocidade, tempo, status e tipo de labirinto. |
| **Pré-condições** | Backend em execução; cliente WebSocket ou teste automatizado disponível. |
| **Procedimentos** | 1. Enviar payload mínimo válido.<br>2. Enviar payload completo típico.<br>3. Enviar payload completo representando labirinto 16x16.<br>4. Verificar logs e validação de schema. |
| **Resultado Esperado** | O backend aceita mensagens válidas e disponibiliza evento para retransmissão. |
| **Reparo** | Corrigir schema, parser ou normalização dos campos de telemetria. |
| **Pós-Reparo** | Reexecutar os três payloads válidos. |

### CT-18 — Rejeição de Pacote Malformado

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que payload inválido é rejeitado sem derrubar o backend. |
| **Requisito(s) coberto(s)** | RF-08 / US08; RF-09 / US09; RNF-09 |
| **Técnica de teste** | Teste negativo + partição de payload inválido |
| **Partições e fronteiras** | Campo ausente, tipo incorreto e payload vazio. |
| **Casos concretos** | Sem `tipo_labirinto`; bateria como texto; `{}`. |
| **Pré-condições** | Backend em execução; handler WebSocket de telemetria acessível. |
| **Procedimentos** | 1. Enviar payload com campo obrigatório ausente.<br>2. Enviar payload com tipo incorreto.<br>3. Enviar payload vazio.<br>4. Consultar saúde/log do backend após cada envio. |
| **Resultado Esperado** | O backend retorna erro controlado, não retransmite dado inválido e permanece operacional. |
| **Reparo** | Fortalecer validação de schema e tratamento de exceções. |
| **Pós-Reparo** | Reexecutar os três payloads inválidos e um payload válido. |

### CT-19 — Ordem: Retransmitir Antes de Persistir

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o backend retransmite pacote final ao dashboard antes ou no mesmo instante da persistência. |
| **Requisito(s) coberto(s)** | RF-08 / US08; RF-13 / US13; RNF-01; RNF-05 |
| **Técnica de teste** | Teste de integração por ordem temporal |
| **Partições e fronteiras** | Cliente conectado, flag de conclusão e escrita no banco. |
| **Casos concretos** | Payload final com `concluido=true` e cliente WebSocket ativo. |
| **Pré-condições** | Backend, WebSocket e banco em execução; registro de data_hora habilitado. |
| **Procedimentos** | 1. Conectar cliente WebSocket.<br>2. Enviar pacote final.<br>3. Registrar data_hora de chegada no dashboard.<br>4. Registrar data_hora de insert no banco.<br>5. Comparar ordem temporal. |
| **Resultado Esperado** | A data_hora de recebimento no dashboard é anterior ou igual à data_hora de persistência. |
| **Reparo** | Revisar ordem de chamadas no handler de telemetria. |
| **Pós-Reparo** | Reexecutar pacote final com cliente conectado. |

### CT-20 — Persistência ao Concluir Desafio

| Atributo | Descrição |
|---|---|
| **Objetivo** | Garantir que o banco persiste somente o resumo final após flag de conclusão. |
| **Requisito(s) coberto(s)** | RF-13 / US13; RNF-05; RNF-06; RNF-10 |
| **Técnica de teste** | Teste de transição por flag de conclusão |
| **Partições e fronteiras** | Pacotes intermediários e pacote final. |
| **Casos concretos** | Três pacotes sem flag; um pacote com `concluido=true`. |
| **Pré-condições** | Banco limpo; backend em execução. |
| **Procedimentos** | 1. Enviar três pacotes sem conclusão.<br>2. Consultar banco após cada envio.<br>3. Enviar pacote final com conclusão.<br>4. Consultar banco novamente. |
| **Resultado Esperado** | Nenhum registro é criado antes da flag; exatamente um resumo final é inserido após a flag. |
| **Reparo** | Corrigir condição de persistência ou controle de duplicidade. |
| **Pós-Reparo** | Reexecutar sequência completa com banco limpo. |

### CT-21 — Corrida Interrompida Não Persiste

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar que uma corrida interrompida antes da conclusão não gera registro parcial. |
| **Requisito(s) coberto(s)** | RF-13 / US13; RNF-10 |
| **Técnica de teste** | Teste negativo de interrupção |
| **Partições e fronteiras** | Stream normal, interrupção e ausência de flag final. |
| **Casos concretos** | Enviar pacotes e fechar conexão antes de `concluido=true`. |
| **Pré-condições** | Banco limpo; backend em execução. |
| **Procedimentos** | 1. Enviar pacotes de telemetria de uma corrida em andamento.<br>2. Encerrar conexão antes da flag de conclusão.<br>3. Consultar banco de dados. |
| **Resultado Esperado** | Nenhum registro parcial é criado. |
| **Reparo** | Corrigir criação prematura de registros ou limpeza de sessão interrompida. |
| **Pós-Reparo** | Reexecutar interrupção e uma corrida concluída. |

### CT-22 — Consulta Filtrada por Labirinto

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a API retorna apenas corridas do tipo de labirinto solicitado. |
| **Requisito(s) coberto(s)** | RF-14 / US14; RNF-07 |
| **Técnica de teste** | Partição de equivalência por filtro |
| **Partições e fronteiras** | Filtros 4x4, 8x8 e 16x16. |
| **Casos concretos** | Base com registros dos três tipos; consultas `4x4`, `8x8` e `16x16`. |
| **Pré-condições** | Banco populado com massa conhecida. |
| **Procedimentos** | 1. Consultar com filtro 4x4.<br>2. Consultar com filtro 8x8.<br>3. Consultar com filtro 16x16.<br>4. Comparar cada resposta com a massa do banco. |
| **Resultado Esperado** | Cada resposta contém somente registros do tipo solicitado. |
| **Reparo** | Corrigir query, parâmetro de filtro ou serialização da resposta. |
| **Pós-Reparo** | Reexecutar os três filtros. |

### CT-23 — Consulta com Filtro "Todos"

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a API retorna todo o histórico quando o filtro geral é usado. |
| **Requisito(s) coberto(s)** | RF-14 / US14 |
| **Técnica de teste** | Teste de partição especial |
| **Partições e fronteiras** | Filtro ausente e parâmetro `todos`. |
| **Casos concretos** | Chamada sem filtro; chamada com `tipo=todos`. |
| **Pré-condições** | Banco com registros de ao menos dois tipos de labirinto. |
| **Procedimentos** | 1. Chamar endpoint sem filtro.<br>2. Chamar endpoint com `tipo=todos`.<br>3. Comparar quantidade retornada com o total no banco. |
| **Resultado Esperado** | As duas chamadas retornam todos os registros esperados. |
| **Reparo** | Corrigir interpretação do filtro geral. |
| **Pós-Reparo** | Reexecutar chamadas sem filtro e com `todos`. |

### CT-24 — Consulta com Banco Vazio

| Atributo | Descrição |
|---|---|
| **Objetivo** | Garantir que consulta sem registros retorna lista vazia em vez de erro. |
| **Requisito(s) coberto(s)** | RF-14 / US14 |
| **Técnica de teste** | Teste de fronteira de coleção vazia |
| **Partições e fronteiras** | Consulta geral vazia e consulta filtrada vazia. |
| **Casos concretos** | Banco sem corridas; consulta sem filtro; consulta com tipo específico. |
| **Pré-condições** | Banco inicializado e sem registros de corrida. |
| **Procedimentos** | 1. Limpar ou usar banco vazio.<br>2. Chamar endpoint de consulta geral.<br>3. Chamar endpoint com filtro de labirinto específico.<br>4. Registrar status HTTP e corpo das respostas. |
| **Resultado Esperado** | A API retorna `[]` com status 200 nas duas consultas, sem erro 500 ou exceção. |
| **Reparo** | Corrigir tratamento de resultado vazio na query ou serialização. |
| **Pós-Reparo** | Reexecutar consulta geral e consulta filtrada em banco vazio. |

---

## Dashboard

Os testes de dashboard verificam a interface responsável pela apresentação da telemetria em tempo real, pela sinalização dos estados de conexão e pela consulta ao histórico de corridas. Os casos avaliam a integração com o backend e a capacidade da interface de manter visibilidade, atualização e filtragem coerentes com os requisitos do sistema.

### CT-25 — Conexão WebSocket

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o dashboard abre conexão WebSocket automaticamente. |
| **Requisito(s) coberto(s)** | RF-08 / US08; RF-10 / US10; RE-04; RE-07 |
| **Técnica de teste** | Teste de integração frontend-backend |
| **Partições e fronteiras** | Dashboard carregado com WebSocket ativo. |
| **Casos concretos** | Abrir dashboard no navegador com backend em execução. |
| **Pré-condições** | Dashboard acessível no navegador; backend em execução com WebSocket ativo. |
| **Procedimentos** | 1. Abrir o dashboard no navegador.<br>2. Verificar log de conexão no servidor.<br>3. Verificar indicador de status no dashboard, quando existir. |
| **Resultado Esperado** | A conexão WebSocket é estabelecida sem interação manual, e o servidor registra o cliente conectado. |
| **Reparo** | Corrigir URL WebSocket, configuração de CORS ou ciclo de inicialização do cliente. |
| **Pós-Reparo** | Recarregar a página e confirmar reconexão automática. |

### CT-26 — Exibição dos 6 Campos de Telemetria

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se todos os seis campos obrigatórios aparecem e atualizam em tempo real. |
| **Requisito(s) coberto(s)** | RF-09 / US09; RF-10 / US10; RF-11 / US11; RF-12 / US12; RNF-11 |
| **Técnica de teste** | Teste baseado em requisitos de interface |
| **Partições e fronteiras** | Todos os campos obrigatórios e atualização sucessiva. |
| **Casos concretos** | Tipo do labirinto, trajeto, bateria, velocidade média, tempo de conclusão e desafio cumprido S/N. |
| **Pré-condições** | Dashboard conectado ao backend ou servidor mock de WebSocket. |
| **Procedimentos** | 1. Iniciar stream de telemetria com todos os campos.<br>2. Verificar no dashboard a presença dos seis campos obrigatórios.<br>3. Enviar novo pacote com valores alterados.<br>4. Confirmar atualização em tempo real. |
| **Resultado Esperado** | Os seis campos ficam visíveis e atualizam conforme os pacotes recebidos. |
| **Reparo** | Corrigir binding de dados ou mapeamento de campos no frontend. |
| **Pós-Reparo** | Reexecutar pacote inicial e pacote atualizado. |

### CT-27 — Exibição de Dados Consolidados ao Fim da Corrida

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que a interface muda de tempo real para resumo final após conclusão. |
| **Requisito(s) coberto(s)** | RF-11 / US11; RF-13 / US13 |
| **Técnica de teste** | Teste de transição de estado da interface |
| **Partições e fronteiras** | Stream em andamento e pacote final. |
| **Casos concretos** | Pacotes contínuos seguidos de `concluido=true`. |
| **Pré-condições** | Dashboard conectado; mock ou backend capaz de enviar stream controlado. |
| **Procedimentos** | 1. Enviar pacotes de corrida em andamento.<br>2. Verificar tela em modo tempo real.<br>3. Enviar pacote final com conclusão.<br>4. Registrar mudança de tela/estado. |
| **Resultado Esperado** | O dashboard apresenta resumo consolidado automaticamente, sem ação manual. |
| **Reparo** | Corrigir tratamento da flag de conclusão ou estado visual da interface. |
| **Pós-Reparo** | Reexecutar stream seguido de conclusão. |

### CT-28 — Filtro por Labirinto Específico

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a seleção de filtro no dashboard consulta e exibe somente o labirinto escolhido. |
| **Requisito(s) coberto(s)** | RF-14 / US14; RNF-07 |
| **Técnica de teste** | Partição de equivalência por seleção de UI |
| **Partições e fronteiras** | Filtros 4x4, 8x8 e 16x16. |
| **Casos concretos** | Selecionar 4x4, 8x8 e 16x16. |
| **Pré-condições** | API com massa de histórico conhecida; dashboard carregado. |
| **Procedimentos** | 1. Selecionar filtro 4x4.<br>2. Verificar requisição e registros exibidos.<br>3. Repetir para 8x8 e 16x16. |
| **Resultado Esperado** | A requisição contém o filtro correto e a listagem não mistura tipos de labirinto. |
| **Reparo** | Corrigir estado do seletor, parâmetro enviado ou renderização da lista. |
| **Pós-Reparo** | Reexecutar os três filtros. |

### CT-29 — Filtro "Todos os Labirintos"

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a opção "Todos" exibe o histórico completo. |
| **Requisito(s) coberto(s)** | RF-14 / US14 |
| **Técnica de teste** | Teste de partição especial |
| **Partições e fronteiras** | Todos selecionado e filtro removido. |
| **Casos concretos** | Selecionar opção `Todos`. |
| **Pré-condições** | Banco/API com registros de mais de um tipo de labirinto. |
| **Procedimentos** | 1. Selecionar opção `Todos` no dashboard.<br>2. Registrar requisição feita pela interface.<br>3. Comparar registros exibidos com a resposta da API. |
| **Resultado Esperado** | Todos os registros retornados pela API são exibidos. |
| **Reparo** | Corrigir valor do filtro geral ou lógica de listagem. |
| **Pós-Reparo** | Reexecutar opção `Todos`. |

### CT-30 — Reconexão WebSocket

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar comportamento do dashboard após queda e retorno da conexão WebSocket. |
| **Requisito(s) coberto(s)** | RF-18 / US18; RNF-01; RNF-02 |
| **Técnica de teste** | Teste de falha e recuperação |
| **Partições e fronteiras** | Conectado, desconectado, reconectando e reconectado. |
| **Casos concretos** | Derrubar backend temporariamente e restaurar em seguida. |
| **Pré-condições** | Dashboard conectado; backend em execução; mecanismo de reconexão implementado. |
| **Procedimentos** | 1. Abrir dashboard conectado.<br>2. Interromper backend ou WebSocket.<br>3. Observar estado visual de perda.<br>4. Restaurar backend.<br>5. Medir tempo até nova atualização. |
| **Resultado Esperado** | A interface sinaliza a queda e volta a receber dados em até 30 segundos, sem recarregar a página. |
| **Reparo** | Corrigir lógica de reconexão, backoff ou atualização de estado visual. |
| **Pós-Reparo** | Repetir queda e restauração. |

---

## Requisitos Não-Funcionais

Os testes não-funcionais explicitam critérios mensuráveis de desempenho, capacidade, integridade e compatibilidade. Esses casos complementam a validação funcional ao estabelecer limites objetivos para latência, taxa de atualização da interface, tempo de carregamento, persistência, volume de dados, conexões simultâneas e funcionamento nos navegadores suportados.

### CT-31 — Latência de Telemetria

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se a telemetria chega ao dashboard em até 500 ms na rede local. |
| **Requisito(s) coberto(s)** | RNF-01; RF-08 / US08 |
| **Técnica de teste** | Análise de fronteira temporal |
| **Partições e fronteiras** | Abaixo do limite (ex: 499 ms), no valor limite exato (500 ms) e acima do limite superior (ex: 501 ms). |
| **Casos concretos** | 30 pacotes com data_hora; atenção a 499 ms, 500 ms e 501 ms. |
| **Pré-condições** | Firmware/simulador, backend e dashboard com registros de data_hora sincronizados ou comparáveis. |
| **Procedimentos** | 1. Enviar 30 pacotes com data_hora de origem.<br>2. Registrar data_hora de exibição no dashboard.<br>3. Calcular latência de cada amostra.<br>4. Comparar com o limite de 500 ms. |
| **Resultado Esperado** | A latência máxima medida é menor ou igual a 500 ms. |
| **Reparo** | Otimizar serialização, WebSocket, fila do backend ou renderização. |
| **Pós-Reparo** | Reexecutar as 30 amostras. |

### CT-32 — Taxa de Atualização da Interface

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que os campos da interface atualizam em intervalo menor ou igual a 1 segundo. |
| **Requisito(s) coberto(s)** | RNF-02; RF-10 / US10; RF-12 / US12 |
| **Técnica de teste** | Análise de fronteira temporal |
| **Partições e fronteiras** | 0,5 s, 1,0 s e intervalo maior que 1,0 s. |
| **Casos concretos** | Stream com contador incremental por 30 segundos. |
| **Pré-condições** | Dashboard conectado ao backend ou mock de stream. |
| **Procedimentos** | 1. Enviar pacotes com contador incremental.<br>2. Registrar instante em que cada valor aparece na tela.<br>3. Calcular intervalo entre atualizações visíveis. |
| **Resultado Esperado** | Nenhum intervalo visível ultrapassa 1 segundo. |
| **Reparo** | Corrigir frequência de envio, processamento do backend ou renderização do frontend. |
| **Pós-Reparo** | Reexecutar stream por 30 segundos. |

### CT-33 — Tempo de Carregamento Inicial

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o dashboard fica interativo em até 3 segundos. |
| **Requisito(s) coberto(s)** | RNF-03; RE-07 |
| **Técnica de teste** | Análise de fronteira temporal |
| **Partições e fronteiras** | Abaixo do limite (ex: 2,9 s), no valor limite exato (3,0 s) e acima do limite superior (ex: 3,1 s). |
| **Casos concretos** | Três carregamentos com cache limpo. |
| **Pré-condições** | Servidor local disponível; navegador com cache limpo. |
| **Procedimentos** | 1. Limpar cache.<br>2. Abrir dashboard.<br>3. Medir tempo até estado interativo.<br>4. Repetir três vezes. |
| **Resultado Esperado** | Todas as execuções atingem estado interativo em até 3 segundos. |
| **Reparo** | Reduzir arquivos estáticos, adiar carregamentos não críticos ou remover dependências desnecessárias. |
| **Pós-Reparo** | Repetir três medições com cache limpo. |

### CT-34 — Ciclo de Controle do Firmware

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o loop principal do firmware executa em até 10 ms. |
| **Requisito(s) coberto(s)** | RNF-04; RF-07 / US07 |
| **Técnica de teste** | Análise de fronteira temporal + teste estrutural |
| **Partições e fronteiras** | 9 ms, 10 ms, 11 ms; cenário leve e cenário 16x16. |
| **Casos concretos** | 100 ciclos consecutivos em fixture 16x16. |
| **Pré-condições** | Firmware instrumentado com medição de início/fim de ciclo. |
| **Procedimentos** | 1. Ativar medição de tempo no loop.<br>2. Executar fixture 16x16.<br>3. Registrar 100 ciclos consecutivos.<br>4. Comparar máximo medido com 10 ms. |
| **Resultado Esperado** | Nenhum ciclo registrado ultrapassa 10 ms. |
| **Reparo** | Remover bloqueios, reduzir delays ou otimizar rotinas críticas. |
| **Pós-Reparo** | Reexecutar 100 ciclos no mesmo fixture. |

### CT-35 — Tempo de Escrita no Banco

| Atributo | Descrição |
|---|---|
| **Objetivo** | Medir se a persistência final ocorre em até 2 segundos após conclusão. |
| **Requisito(s) coberto(s)** | RNF-05; RF-13 / US13 |
| **Técnica de teste** | Análise de fronteira temporal |
| **Partições e fronteiras** | Abaixo do limite (ex: 1,9 s), no valor limite exato (2,0 s) e acima do limite superior (ex: 2,1 s). |
| **Casos concretos** | Pacote final com data_hora de recebimento e insert. |
| **Pré-condições** | Backend e banco em execução; logs de persistência habilitados. |
| **Procedimentos** | 1. Enviar pacote final.<br>2. Registrar data_hora de recebimento.<br>3. Registrar data_hora de insert.<br>4. Calcular diferença. |
| **Resultado Esperado** | A diferença entre conclusão e insert é menor ou igual a 2 segundos. |
| **Reparo** | Otimizar transação, serialização do resumo ou fluxo assíncrono. |
| **Pós-Reparo** | Reexecutar três pacotes finais. |

### CT-36 — Tamanho do Resumo Persistido

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se cada resumo de corrida salvo tem no máximo 10 KB. |
| **Requisito(s) coberto(s)** | RNF-06; RF-13 / US13 |
| **Técnica de teste** | Análise de fronteira de tamanho |
| **Partições e fronteiras** | 4x4, 8x8, pior caso 16x16; limite de 10 KB. |
| **Casos concretos** | Resumos serializados para 4x4, 8x8 e 16x16. |
| **Pré-condições** | Backend capaz de gerar ou persistir resumos dos três tamanhos. |
| **Procedimentos** | 1. Gerar resumo 4x4.<br>2. Gerar resumo 8x8.<br>3. Gerar resumo 16x16 em pior caso previsto.<br>4. Medir tamanho serializado. |
| **Resultado Esperado** | Cada resumo medido tem tamanho menor ou igual a 10 KB. |
| **Reparo** | Compactar matriz/trajeto ou remover campos redundantes. |
| **Pós-Reparo** | Reexecutar principalmente o cenário 16x16. |

### CT-37 — Consulta com 100 Corridas

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o banco consulta histórico filtrado com 100 corridas em até 1 segundo. |
| **Requisito(s) coberto(s)** | RNF-07; RF-14 / US14 |
| **Técnica de teste** | Teste de carga leve + análise de fronteira de volume |
| **Partições e fronteiras** | 99, 100 e 101 registros; filtros por tipo. |
| **Casos concretos** | Massa com 100 corridas distribuídas entre 4x4, 8x8 e 16x16. |
| **Pré-condições** | Banco populado com massa conhecida. |
| **Procedimentos** | 1. Popular banco com 100 corridas.<br>2. Consultar por 4x4, 8x8 e 16x16.<br>3. Medir tempo de resposta.<br>4. Validar conteúdo retornado. |
| **Resultado Esperado** | Todas as consultas filtradas respondem em até 1 segundo e retornam registros corretos. |
| **Reparo** | Criar índice, otimizar query ou revisar serialização da resposta. |
| **Pós-Reparo** | Reexecutar consultas com a mesma massa. |

### CT-38 — Dez Clientes Simultâneos

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o WebSocket mantém 10 dashboards atualizados simultaneamente. |
| **Requisito(s) coberto(s)** | RNF-08; RF-10 / US10; RF-18 / US18 |
| **Técnica de teste** | Teste de carga de conexão WebSocket |
| **Partições e fronteiras** | 1 cliente, 10 clientes e acima de 10 como estresse opcional. |
| **Casos concretos** | 10 clientes recebendo o mesmo stream por 60 segundos. |
| **Pré-condições** | Backend WebSocket ativo; ferramenta ou script para simular clientes. |
| **Procedimentos** | 1. Abrir 10 conexões WebSocket.<br>2. Iniciar stream de telemetria.<br>3. Registrar recebimento em cada cliente por 60 segundos.<br>4. Verificar quedas e atraso. |
| **Resultado Esperado** | Todos os clientes permanecem conectados e recebem atualizações em até 1 segundo. |
| **Reparo** | Corrigir broadcast, filas assíncronas ou limites de conexão. |
| **Pós-Reparo** | Reexecutar teste com 10 clientes por 60 segundos. |

**CT-39 — Tamanho do Pacote de Telemetria**

| Atributo | Descrição |
| :--- | :--- |
| **Objetivo** | Verificar se os pacotes da transmissão contínua respeitam o limite de 512 bytes. O pacote de finalização é exceção documentada (RNF-09) e não está sujeito ao limite. |
| **Requisito(s) coberto(s)** | RNF-09; RF-08 / US08; RF-09 / US09 |
| **Técnica de teste** | Análise de fronteira de tamanho |
| **Partições e fronteiras** | Transmissão contínua: mínimo, típico e máximo; fronteiras em 511, 512 e 513 bytes. Pacote de finalização: registrado apenas para referência, fora da partição do limite. Escopo: labirintos 4x4 e 8x8. |
| **Casos concretos** | Pacote mínimo (`running`, 1º passo), típico (`running`, meio da corrida) e máximo da transmissão contínua nos labirintos 4x4 e 8x8; pacote de finalização completo (`path_traversed` + `known_walls`) registrado para referência. |
| **Pré-condições** | `scripts/fake_robot.py` disponível e funcional; serializador de telemetria acessível. |
| **Procedimentos** | 1. Serializar pacote `running` mínimo e registrar tamanho. 2. Serializar pacote `running` típico e registrar tamanho. 3. Serializar pacote `running` máximo nos labirintos 4x4 e 8x8 e validar ≤ 512 bytes. 4. Serializar o pacote de finalização completo nos mesmos labirintos e registrar o tamanho sem aplicar o limite. 5. Verificar que pacotes contínuos acima de 512 bytes são rejeitados pelo backend. |
| **Resultado Esperado** | Todos os pacotes com `race_status` diferente de `finished` têm no máximo 512 bytes. O pacote de finalização pode exceder esse limite sem rejeição. Pacotes contínuos acima do limite são rejeitados. |
| **Reparo** | Reduzir campos ou compactar representação — aplicável apenas aos pacotes da transmissão contínua. |
| **Pós-Reparo** | Reexecutar os passos 3 e 5. |

### CT-40 — Integridade Somente-Leitura

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se dados persistidos não podem ser editados ou apagados pela interface pública. |
| **Requisito(s) coberto(s)** | RNF-10; RF-13 / US13 |
| **Técnica de teste** | Teste negativo de segurança funcional |
| **Partições e fronteiras** | PUT, PATCH, DELETE e sobrescrita indevida. |
| **Casos concretos** | Tentar alterar e excluir corrida existente. |
| **Pré-condições** | Banco com ao menos uma corrida persistida; API em execução. |
| **Procedimentos** | 1. Identificar registro existente.<br>2. Tentar PUT, PATCH e DELETE pela API pública.<br>3. Consultar o registro após as tentativas. |
| **Resultado Esperado** | As tentativas são rejeitadas e o registro permanece inalterado. |
| **Reparo** | Remover endpoint indevido ou aplicar bloqueio/validação de autorização. |
| **Pós-Reparo** | Reexecutar tentativas de alteração e exclusão. |

### CT-41 — Compatibilidade de Navegadores

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar funcionamento do dashboard nos navegadores suportados. |
| **Requisito(s) coberto(s)** | RNF-12; RE-08 |
| **Técnica de teste** | Partição por ambiente de execução |
| **Partições e fronteiras** | Chrome, Firefox e Edge nas versões suportadas. |
| **Casos concretos** | Chrome >= 110, Firefox >= 110 e Edge >= 110. |
| **Pré-condições** | Backend ativo; massa de teste disponível; navegadores instalados. |
| **Procedimentos** | 1. Abrir dashboard no Chrome.<br>2. Validar conexão, atualização e filtro.<br>3. Repetir no Firefox.<br>4. Repetir no Edge. |
| **Resultado Esperado** | O fluxo principal funciona nos três navegadores sem plugins adicionais. |
| **Reparo** | Corrigir incompatibilidade de JavaScript, CSS ou APIs do navegador. |
| **Pós-Reparo** | Reexecutar no navegador afetado e em pelo menos mais um. |

---

## Funcionalidades Adicionais

Os testes de funcionalidades adicionais cobrem recursos classificados como complementares no escopo do produto. Embora não sejam indispensáveis ao funcionamento mínimo do Micromouse, esses casos estruturam a validação de funcionalidades que ampliam a análise histórica dos dados, a exportação dos resultados e a revisão visual das corridas já registradas.

### CT-42 — Ranking de Melhores Corridas

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o ranking exibe Top 5 por menor tempo e por tipo de labirinto. |
| **Requisito(s) coberto(s)** | RF-15 / US15 |
| **Técnica de teste** | Partição de equivalência por status e ordenação |
| **Partições e fronteiras** | Corridas concluídas, não concluídas e mais de cinco registros por tipo. |
| **Casos concretos** | Base com 6 ou mais corridas para 4x4, 8x8 e 16x16. |
| **Pré-condições** | Ranking implementado; banco com massa controlada. |
| **Procedimentos** | 1. Popular banco com corridas concluídas e não concluídas.<br>2. Abrir ranking.<br>3. Conferir quantidade, ordenação e filtro por status em cada tipo. |
| **Resultado Esperado** | Cada labirinto exibe no máximo 5 corridas concluídas, ordenadas por menor tempo. |
| **Reparo** | Corrigir filtro por status, ordenação ou limite da consulta. |
| **Pós-Reparo** | Reexecutar ranking com a mesma massa. |

### CT-43 — Exportação do Histórico em CSV

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o CSV preserva filtro ativo e campos obrigatórios do histórico. |
| **Requisito(s) coberto(s)** | RF-16 / US16; RNF-06 |
| **Técnica de teste** | Partição por filtro ativo + validação de formato |
| **Partições e fronteiras** | Exportação 4x4, 8x8, 16x16 e Todos. |
| **Casos concretos** | Exportar CSV com cada filtro. |
| **Pré-condições** | Banco com registros em mais de um tipo de labirinto; exportação implementada. |
| **Procedimentos** | 1. Selecionar filtro 4x4 e exportar.<br>2. Repetir para 8x8, 16x16 e Todos.<br>3. Validar cabeçalho, registros e filtro aplicado. |
| **Resultado Esperado** | O CSV contém tempo, trajeto, velocidade, bateria, status, tipo e data_hora coerentes com a API. |
| **Reparo** | Corrigir serialização, cabeçalho, separador ou aplicação do filtro. |
| **Pós-Reparo** | Reexportar os quatro cenários. |

### CT-44 — Replay de Corrida Salva

| Atributo | Descrição |
|---|---|
| **Objetivo** | Verificar se o replay reproduz a sequência persistida sem perder estado nos controles. |
| **Requisito(s) coberto(s)** | RF-17 / US17 |
| **Técnica de teste** | Teste de sequência + transição de estado da interface |
| **Partições e fronteiras** | Trajeto curto, curvas, revisitas, play, pause e velocidade. |
| **Casos concretos** | Corrida curta; corrida com curvas; corrida com revisita. |
| **Pré-condições** | Banco com corridas contendo sequência de células persistida; replay implementado. |
| **Procedimentos** | 1. Abrir replay de corrida curta.<br>2. Acionar play e pause.<br>3. Alterar velocidade.<br>4. Repetir com corrida com curvas e corrida com revisita.<br>5. Comparar animação com trajeto salvo. |
| **Resultado Esperado** | A animação segue a ordem das células salvas e os controles não reiniciam a posição indevidamente. |
| **Reparo** | Corrigir parser do trajeto, estado da animação ou vínculo com histórico. |
| **Pós-Reparo** | Reexecutar os três tipos de corrida. |

---

## Conformidade com o Tema PI1

Este grupo explicita a aderência do software às regras centrais do Tema PI1 que afetam a representação lógica do labirinto. A validação considera a escala das células, os tamanhos oficiais de labirinto e a definição da área central como objetivo, restringindo-se aos aspectos lógicos sem absorver verificações físicas de construção da pista, que pertencem a outro artefato.

### CT-45 — Geometria Lógica e Objetivo Central

| Atributo | Descrição |
|---|---|
| **Objetivo** | Validar que o software representa corretamente a geometria lógica do labirinto e a sala central 2x2. |
| **Requisito(s) coberto(s)** | RF-04 / US04; RF-05 / US05; Tema PI1 |
| **Técnica de teste** | Teste baseado em especificação + partição de equivalência + análise de fronteira |
| **Partições e fronteiras** | Labirintos 4x4, 8x8 e 16x16; posição fora do centro, adjacente ao centro e dentro do centro; deslocamento de 0, 1 e 2 células. |
| **Casos concretos** | Conversões 0 cm, 18 cm e 36 cm; célula externa adjacente ao centro; quatro células centrais de cada tamanho. |
| **Pré-condições** | Constantes de geometria e função de detecção de objetivo acessíveis por teste unitário, simulador ou log do firmware. |
| **Procedimentos** | 1. Carregar configuração 4x4, 8x8 e 16x16.<br>2. Validar conversão de 0, 1 e 2 células para centímetros.<br>3. Simular posição adjacente externa ao centro e consultar estado de objetivo.<br>4. Simular cada uma das quatro células centrais e consultar estado de objetivo.<br>5. Repetir para os três tamanhos de labirinto. |
| **Resultado Esperado** | O software usa 18 cm como unidade de célula, escala corretamente os três labirintos e só sinaliza sucesso quando a posição pertence à sala central 2x2. |
| **Reparo** | Corrigir constantes de geometria, conversão célula-centímetro ou cálculo das coordenadas centrais. |
| **Pós-Reparo** | Reexecutar conversões e detecção de objetivo nos três tamanhos. |

---

## Cobertura Resumida

A cobertura resumida relaciona os grupos de requisitos aos casos de teste que os validam, viabilizando a verificação imediata da rastreabilidade entre navegação, telemetria, persistência, interface e funcionalidades adicionais.

| Grupo de requisito | Casos principais |
|---|---|
| Navegação, localização e mapeamento | CT-01 a CT-16, CT-45 |
| Telemetria em tempo real | CT-17 a CT-19, CT-25 a CT-27, CT-31, CT-32, CT-39 |
| Persistência e consulta | CT-20 a CT-24, CT-28, CT-29, CT-35 a CT-37, CT-40 |
| Interface web e qualidade de conexão | CT-25 a CT-30, CT-38, CT-41 |
| Funcionalidades adicionais | CT-42 a CT-44 |
