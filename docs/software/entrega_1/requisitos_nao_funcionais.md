# Especificação de Requisitos Não-Funcionais

## 1. Introdução

O sistema do projeto Micromouse estrutura-se em dois blocos integrados: o firmware embarcado no Micromouse, responsável pelo controle autônomo do robô, e a aplicação web, encarregada de exibir a telemetria em tempo real e de armazenar os dados de cada corrida em banco de dados. Os requisitos não-funcionais consolidados neste documento estabelecem os atributos de qualidade necessários para que ambos os blocos operem com confiabilidade, desempenho e integridade compatíveis com o contexto acadêmico de uso.

## 2. Desempenho: Métricas de Tempo

### RNF-01 — Latência de Transmissão de Telemetria

| Atributo | Valor |
|---|---|
| **Descrição** | O Micromouse deve transmitir dados de telemetria ao sistema web com latência máxima aceitável |
| **Métrica** | Tempo entre geração do dado no robô e exibição na interface web |
| **Limite máximo** | ≤ 500 ms (em condições normais de operação na rede local) |
| **Condição de medição** | Rede Wi-Fi local, distância de até 10 metros entre robô e roteador |
| **Justificativa** | Telemetria em tempo real, conforme percebida pelo usuário, exige atualização sub-segundo |
| **MoSCoW** | **Must Have**. Sem latência sub-segundo, a interface deixa de atender ao requisito de tempo real previsto no enunciado |

### RNF-02 — Taxa de Atualização da Interface Web

| Atributo | Valor |
|---|---|
| **Descrição** | A interface web deve atualizar os dados exibidos (velocidade, trajeto, bateria) com frequência mínima |
| **Métrica** | Intervalo entre atualizações consecutivas dos dados na tela |
| **Limite máximo** | ≤ 1 segundo por ciclo de atualização |
| **Condição de medição** | Navegador moderno (Chrome/Firefox), conexão local ativa |
| **Justificativa** | Assegura que o observador acompanhe o progresso do Micromouse em tempo real |
| **MoSCoW** | **Must Have**. Complementa o RNF-01; em conjunto, ambos definem a percepção de tempo real exigida pelo enunciado |

A interface deve atualizar-se a cada 1 segundo. Para viabilizar essa cadência, a transmissão dos dados do robô ao servidor precisa ser concluída em tempo inferior a esse intervalo, deixando margem para o processamento no servidor e a renderização no navegador. Adotando uma divisão conservadora (metade do tempo para transmissão e metade para processamento e renderização), chega-se a 500 ms como limite para a etapa de transmissão. Em redes locais Wi-Fi típicas, as latências costumam situar-se entre 10 e 50 ms, de modo que o limite estabelecido é amplamente seguro e alcançável.

### RNF-03 — Tempo de Carregamento da Página Web

| Atributo | Valor |
|---|---|
| **Descrição** | A aplicação web deve carregar completamente dentro de um tempo aceitável |
| **Métrica** | Tempo desde a requisição HTTP até o estado "interativo" da página (TTI) |
| **Limite máximo** | ≤ 3 segundos (em rede local) |
| **Condição de medição** | Conexão local, hardware padrão de laboratório |
| **Justificativa** | Evita espera excessiva antes do início da corrida |
| **MoSCoW** | **Should Have**. Trata-se de ajuste de qualidade de uso; um carregamento de 4 a 5 segundos não inviabiliza a corrida, ainda que degrade a experiência durante apresentações |

Este requisito se aplica ao carregamento inicial da aplicação, não à telemetria em si. O limite de 3 segundos é amplamente adotado como referência na indústria, sendo definido pelas Core Web Vitals do Google como o intervalo aceitável para o estado interativo.

### RNF-04 — Tempo de Ciclo de Controle do Firmware

| Atributo | Valor |
|---|---|
| **Descrição** | Tempo máximo de execução do loop principal de controle no microcontrolador |
| **Limite máximo** | ≤ 10 ms por ciclo (equivalente a ≥ 100 Hz) |
| **Condição de medição** | Execução no microcontrolador alvo, labirinto 16×16 em operação |
| **MoSCoW** | **Must Have**. Decorre da física do robô (velocidade × tamanho da célula); abaixo de 100 Hz, a navegação torna-se instável e a probabilidade de colisão aumenta |

O valor decorre das características físicas da competição. O robô precisa detectar paredes, corrigir sua trajetória e tomar decisões de navegação enquanto se desloca. As células têm 18 cm de lado e robôs em competições Micromouse tipicamente atingem velocidades entre 0,5 e 2 m/s, o que resulta em um tempo aproximado de 45 a 180 ms para atravessar metade de uma célula. Para garantir pelo menos quatro a cinco ciclos de controle nesse intervalo, assegurando estabilidade, a frequência mínima deve aproximar-se de 100 Hz, ou 10 ms por ciclo. Este requisito deve ser revisado após a definição do microcontrolador.

### RNF-05 — Tempo de Armazenamento no Banco de Dados

| Atributo | Valor |
|---|---|
| **Descrição** | Tempo máximo entre a detecção do objetivo pelo Micromouse e a confirmação de escrita dos dados finais no banco |
| **Limite máximo** | ≤ 2 segundos |
| **Condição de medição** | Banco de dados local ou servidor na mesma rede |
| **MoSCoW** | **Should Have**. Uma escrita ligeiramente mais lenta não compromete a avaliação, mas a janela curta previne a perda de dados caso o operador desligue o sistema logo após a corrida |

Ao término de uma corrida, o sistema deve garantir que todos os dados sejam armazenados antes que o operador desligue o robô ou encerre a aplicação. Dois segundos constituem tempo suficiente para a interface detectar o evento de conclusão, montar o objeto de dados e executar a operação de escrita no banco local.

## 3. Capacidade e Carga

### RNF-06 — Tamanho do Resumo Final por Corrida

| Atributo | Valor |
|---|---|
| **Descrição** | Tamanho máximo dos dados consolidados de uma corrida persistidos no banco |
| **Limite máximo** | ≤ 10 KB por corrida |
| **Conteúdo do resumo** | Tempo total, trajeto percorrido (matriz de paredes e sequência de células), velocidade média, consumo de bateria e status do desafio |
| **MoSCoW** | **Should Have**. Trata-se de limite de dimensionamento que orienta o desenho do schema; ultrapassá-lo não interrompe o sistema, mas evidencia a necessidade de revisão do formato |

Conforme o Diagrama de Atividades (raia do Backend) e a história US13, o backend grava no banco apenas o resumo final da corrida, ao receber a flag de conclusão emitida pelo firmware (comportamento verificado por CT-20 e CT-21). O stream contínuo de telemetria não é persistido: trafega entre firmware, backend e frontend apenas para alimentar a interface em tempo real.

O dimensionamento do resumo considera o pior caso (labirinto 16×16): a matriz de paredes ocupa cerca de 128 bytes (1 bit por parede em 256 células), e a sequência de células percorridas, mesmo com revisitas durante a fase de exploração, fica abaixo de 4 KB quando serializada em formato compacto. Os demais campos (escalares e enum de status) somam menos de 100 bytes. O limite de 10 KB cobre esse cenário com folga, considerando o overhead de serialização em JSON.

### RNF-07 — Total de Corridas Armazenadas

| Atributo | Valor |
|---|---|
| **Descrição** | Quantidade mínima de corridas que o banco de dados deve suportar sem degradação de desempenho nas consultas |
| **Limite mínimo** | ≥ 100 corridas |
| **Tempo de consulta** | Resultado de consulta por labirinto específico em ≤ 1 segundo |
| **MoSCoW** | **Should Have**. O limite cobre o uso estimado do semestre com folga; valores inferiores ainda suportam a apresentação final, mas inviabilizam análise histórica |

Estimando aproximadamente 10 sessões de teste distribuídas ao longo do semestre, com 3 labirintos por sessão e até 3 tentativas cada, chega-se a cerca de 90 corridas no total. O limite de 100 corridas cobre esse uso com pequena margem de segurança. Combinado ao limite do RNF-06 (≤ 10 KB por corrida), o banco precisaria suportar aproximadamente 1 MB de dados, volume sem qualquer impacto para PostgreSQL, SQLite, MySQL ou outros bancos relacionais.

### RNF-08 — Usuários Simultâneos no Sistema Web

| Atributo | Valor |
|---|---|
| **Descrição** | Número mínimo de usuários que podem acessar a interface web simultaneamente sem degradação |
| **Limite mínimo** | ≥ 10 usuários simultâneos |
| **Critério de degradação** | Latência de atualização mantida em ≤ 1 segundo com 10 clientes ativos |
| **MoSCoW** | **Should Have**. Cobre o cenário da apresentação final (aproximadamente 10 pessoas); com menos clientes simultâneos, o sistema continua funcional, apenas reduzindo o número de avaliadores que podem acompanhar pela própria máquina |

O contexto mais exigente é a apresentação final, na qual professores avaliadores e integrantes da equipe podem acompanhar o sistema simultaneamente. Considerando 5 professores avaliadores e aproximadamente 5 a 6 membros do grupo, totalizam-se cerca de 10 a 11 usuários simultâneos no pico de uso. O limite de ≥ 10 conexões simultâneas cobre esse cenário com folga mínima e deve ser explicitamente testado, dado que o sistema de telemetria mantém conexões WebSocket abertas continuamente, o que consome mais recursos do que requisições HTTP convencionais.

### RNF-09 — Tamanho Máximo do Pacote de Telemetria

| Atributo | Valor |
|---|---|
| **Descrição** | Tamanho máximo de cada pacote de dados enviado pelo Micromouse ao servidor por ciclo |
| **Limite máximo** | ≤ 512 bytes por pacote |
| **MoSCoW** | **Should Have**. Restringe o pacote ao buffer típico do ESP32 e evita fragmentação; pacotes maiores funcionam, mas elevam a latência e o risco de perda |

Microcontroladores tipicamente utilizados em projetos Micromouse (como STM32 ou ESP32) possuem buffers de comunicação limitados. Pacotes menores reduzem também a latência de transmissão e o risco de fragmentação na rede. Os dados necessários por ciclo (posição, velocidade, nível de bateria e data_hora) podem ser representados em menos de 100 bytes com estrutura binária eficiente. O limite de 512 bytes é generoso o bastante para acomodar inclusive formatos textuais como JSON, mais convenientes para depuração durante o desenvolvimento.

> O pacote de finalização trafega fora da cadência de 1 Hz e contém campos de tamanho variável dependentes do labirinto (`path_traversed` e `known_walls`); por isso é tratado como exceção. O limite de 512 bytes aplica-se exclusivamente aos pacotes da transmissão contínua.

## 4. Segurança

### RNF-10 — Integridade dos Dados de Corrida

| Atributo | Valor |
|---|---|
| **Descrição** | Os dados gravados no banco após uma corrida não devem poder ser modificados pela interface web |
| **Requisito** | Interface de consulta somente leitura; nenhum endpoint de modificação ou exclusão exposto publicamente |
| **MoSCoW** | **Must Have**. Os dados persistidos servem de evidência para avaliação; a possibilidade de edição comprometeria a credibilidade do registro |

Como os dados são utilizados na avaliação acadêmica, é fundamental garantir que não haja possibilidade de alteração posterior dos resultados, seja acidental, seja intencional. A forma mais simples de assegurar essa propriedade é não implementar endpoints de atualização ou deleção no sistema web, restringindo qualquer eventual correção a uma operação administrativa direta no banco de dados, com acesso controlado.

> **Decisão registrada (10/06/2026):** existe uma única operação administrativa de limpeza, `DELETE /historico` (item 7.25 do cronograma), **restrita à máquina que executa o backend** — requisições originadas de outros hosts da rede recebem `403 Forbidden` (`require_local_request` em `src/backend/app.py`). Nenhum endpoint de edição ou exclusão é acessível publicamente pela rede; a restrição é verificada pelos testes do CT-40 em `src/backend/tests/test_app.py`.

## 5. Usabilidade

### RNF-11 — Visibilidade dos Dados de Telemetria

| Atributo | Valor |
|---|---|
| **Descrição** | Todos os seis campos de telemetria exigidos pelo projeto devem estar visíveis sem necessidade de rolagem |
| **Campos obrigatórios** | Tipo do labirinto, trajeto, consumo de bateria, velocidade média, tempo de conclusão, desafio cumprido (S/N) |
| **Requisito de layout** | Interface responsiva, adaptável a diferentes tamanhos de tela |
| **MoSCoW** | **Must Have**. O enunciado lista explicitamente os seis campos exigidos na interface, que devem permanecer visíveis durante a apresentação |

### RNF-12 — Compatibilidade de Navegadores

| Atributo | Valor |
|---|---|
| **Descrição** | A aplicação deve funcionar corretamente nos navegadores mais utilizados |
| **Requisito** | Chrome ≥ 110, Firefox ≥ 110, Edge ≥ 110 |
| **MoSCoW** | **Should Have**. Suporte aos três principais navegadores evita a dependência de um equipamento específico no dia da avaliação, embora o funcionamento em apenas um deles ainda viabilize a entrega |

A definição de um conjunto mínimo de navegadores suportados assegura o funcionamento do sistema nos equipamentos disponíveis durante as avaliações, independentemente do laboratório ou computador utilizado. As versões ≥ 110 foram escolhidas por oferecerem amplo suporte a APIs modernas como WebSocket, CSS Grid e Fetch API, recursos previstos para o desenvolvimento da interface de telemetria.

# Restrições de Ambiente e Documentação de Software

## 1. Introdução

As restrições de ambiente delimitam as tecnologias e configurações que cada camada do sistema deve adotar. Estão organizadas em quatro grupos: firmware embarcado, backend, frontend e banco de dados. A explicitação dessas restrições assegura a homogeneidade do ambiente entre os integrantes do grupo e a viabilidade de execução no contexto acadêmico.

## 2. Firmware Embarcado

### RE-01 — Microcontrolador

| Atributo | Valor |
|---|---|
| **Tecnologia adotada** | ESP32 |
| **Linguagem** | C/C++ (framework Arduino ou ESP-IDF) |
| **Restrição** | O firmware deve ser compilado e executado exclusivamente no ESP32 |

O ESP32 foi selecionado por integrar Wi-Fi nativamente, condição indispensável para a transmissão de telemetria em tempo real ao sistema web sem a necessidade de módulos adicionais. Trata-se também do microcontrolador com maior disponibilidade de bibliotecas para controle de motores, leitura de sensores infravermelhos e encoders, componentes centrais do Micromouse. Alternativas como o STM32 oferecem maior capacidade de controle em tempo real, mas exigiriam módulo Wi-Fi externo, o que aumentaria a complexidade do hardware.

### RE-02 — Protocolo de Comunicação

| Atributo | Valor |
|---|---|
| **Protocolo** | Wi-Fi (IEEE 802.11 b/g/n) + WebSocket |
| **Restrição** | A comunicação entre robô e servidor deve ocorrer via rede local (LAN) |
| **Frequência mínima de envio** | 1 pacote por segundo durante a corrida |

O WebSocket sobre TCP foi adotado por viabilizar comunicação bidirecional em tempo real com o servidor, mantendo conexão persistente entre firmware e backend. A rede local elimina a dependência de internet durante as apresentações, o que assegura maior confiabilidade e latência compatível com RNF-01 e RNF-02.

### RE-03 — Sensores Suportados

| Atributo | Valor |
|---|---|
| **Sensores de distância** | Infravermelho (IR) ou ultrassônico (HC-SR04 ou similar) |
| **Encoder de rodas** | Encoder incremental compatível com GPIO do ESP32 |
| **Restrição** | O firmware deve ler e processar dados de pelo menos 3 sensores de distância simultaneamente (frente, esquerda e direita) |

A detecção de paredes em três direções constitui o mínimo necessário para que algoritmos de navegação como Flood Fill ou wall-following operem corretamente. O ESP32 dispõe de GPIOs suficientes para essa configuração, dispensando o uso de multiplexação.

## 3. Backend

### RE-04 — Linguagem e Framework

| Atributo | Valor |
|---|---|
| **Linguagem** | Python 3.10 ou superior |
| **Framework** | FastAPI |
| **Restrição** | O backend deve ser executável localmente, sem necessidade de infraestrutura em nuvem |

Python é a linguagem de maior familiaridade entre os integrantes do grupo. O FastAPI foi preferido ao Flask por oferecer suporte nativo a WebSocket, condição essencial para a transmissão de telemetria em tempo real, e por adotar modelo assíncrono por padrão, o que viabiliza o atendimento simultâneo a múltiplas conexões (professores e integrantes durante a apresentação) com melhor desempenho. A execução local elimina dependências externas durante as apresentações.

### RE-05 — Sistema Operacional do Servidor

| Atributo | Valor |
|---|---|
| **Sistemas suportados** | Windows 10/11, Ubuntu 22.04 LTS, macOS 12 ou superior |
| **Restrição** | O backend deve inicializar e operar corretamente em qualquer um dos sistemas listados |

Como os integrantes utilizam diferentes sistemas operacionais em suas máquinas pessoais, a compatibilidade com os três sistemas mais comuns evita que a apresentação dependa de um equipamento específico. Python 3.10+ e FastAPI são multiplataforma por natureza, de modo que essa restrição é atendida sem esforço adicional, desde que as dependências sejam gerenciadas via `requirements.txt` ou ferramenta equivalente.

### RE-06 — Gerenciamento de Dependências

| Atributo | Valor |
|---|---|
| **Ferramenta** | pip + arquivo `requirements.txt` |
| **Restrição** | Todas as dependências do backend devem estar listadas no `requirements.txt` com versões fixadas |

A fixação de versões assegura comportamento idêntico do sistema em qualquer máquina onde for instalado, eliminando inconsistências decorrentes de atualizações silenciosas de pacotes.

## 4. Frontend

### RE-07 — Tecnologias de Interface

| Atributo | Valor |
|---|---|
| **Tecnologias** | React 19, Tailwind CSS |
| **Restrição** | A interface utiliza o ecossistema React (via Create React App) e Tailwind para componentes visuais, exigindo processo de build com Node.js |

O uso de React 19 e Tailwind CSS foi adotado para facilitar a criação de uma interface rica, componentizada e responsiva. Embora adicione a necessidade de um ambiente de build (Node.js, npm, Webpack via CRA), o ganho em produtividade e manutenibilidade para o desenvolvimento do dashboard, histórico e simulador compensa a complexidade adicional. A atualização em tempo real é implementada com WebSocket nativo do navegador, enquanto a Fetch API atende às consultas REST ao histórico.

### RE-08 — Navegadores Suportados

| Atributo | Valor |
|---|---|
| **Navegadores** | Google Chrome ≥ 110, Mozilla Firefox ≥ 110, Microsoft Edge ≥ 110 |
| **Restrição** | A interface deve funcionar corretamente em qualquer um dos navegadores listados, sem plugins adicionais |

As versões ≥ 110 foram escolhidas por assegurarem suporte completo a WebSocket, CSS Grid, Flexbox e Fetch API. Trata-se ainda de versões amplamente adotadas, presentes na maioria dos notebooks atuais sem necessidade de atualização.

### RE-09 — Layout Responsivo

| Atributo | Valor |
|---|---|
| **Restrição** | A interface deve adaptar-se a telas de notebooks e desktops convencionais sem perda de usabilidade |
| **Referência mínima** | Telas com largura a partir de 1024 px |

Durante as apresentações, diferentes dispositivos podem ser utilizados para acessar a interface. Assegurar funcionamento a partir de 1024 px cobre a grande maioria dos notebooks atuais, dispensando a otimização para dispositivos móveis, que não integram o escopo de uso do sistema.

## 5. Banco de Dados

### RE-10 — Sistema de Banco de Dados

| Atributo | Valor |
|---|---|
| **Tecnologia** | SQLite 3 |
| **Restrição** | O banco de dados deve ser um arquivo local, sem necessidade de servidor dedicado |

O SQLite foi adotado por ser um banco baseado em arquivo. Para o volume de dados esperado (aproximadamente 100 corridas, totalizando cerca de 1 MB), o SQLite oferece desempenho mais que suficiente. Sua portabilidade também constitui vantagem operacional, dado que o banco se materializa como um único arquivo passível de cópia, versionamento e compartilhamento entre os integrantes do grupo.

### RE-11 — Acesso ao Banco de Dados

| Atributo | Valor |
|---|---|
| **Biblioteca** | SQLAlchemy (ORM) ou sqlite3 (biblioteca padrão do Python) |
| **Restrição** | O acesso ao banco deve ser realizado exclusivamente pelo backend; a interface web não deve ter acesso direto ao banco |

A centralização do acesso ao banco no backend assegura que toda escrita e leitura percorram uma camada de validação, o que previne inconsistências e atende ao requisito de integridade dos dados (RNF-10). O uso de SQLAlchemy como ORM facilita a manutenção do código e a eventual migração para outro banco, caso necessário.

## 6. Resumo das Restrições Tecnológicas

A síntese a seguir consolida as restrições tecnológicas por camada, registrando a versão mínima associada a cada item.

| ID | Camada | Tecnologia | Versão mínima |
|---|---|---|---|
| RE-01 | Firmware | ESP32 (C/C++) | ESP-IDF 5.0 / Arduino Core 2.0 |
| RE-02 | Firmware | Wi-Fi + WebSocket | IEEE 802.11 b/g/n |
| RE-03 | Firmware | Sensores IR/ultrassônicos + encoder | ≥ 3 sensores de distância (frente, esquerda, direita) |
| RE-04 | Backend | Python + FastAPI | Python 3.10 |
| RE-05 | Backend | Windows / Ubuntu / macOS | Win 10, Ubuntu 22.04, macOS 12 |
| RE-06 | Backend | pip + requirements.txt | Dependências com versões fixadas |
| RE-07 | Frontend | React 19 + Tailwind | CRA (Node.js) |
| RE-08 | Frontend | Chrome / Firefox / Edge | versão 110 |
| RE-09 | Frontend | Layout responsivo | largura ≥ 1024px |
| RE-10 | Banco de dados | SQLite 3 | versão 3.35+ |
| RE-11 | Banco de dados | SQLAlchemy ou sqlite3 | Acesso exclusivo pelo backend |
