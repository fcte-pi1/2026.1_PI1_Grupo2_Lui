# Backend — API de Telemetria do Robô

API REST + WebSocket desenvolvida com **FastAPI** para recebimento, processamento e transmissão em tempo real dos dados de telemetria do micromouse.

## Estrutura

```
backend/
├── app.py                  # Entrypoint da aplicação (rotas HTTP e WebSocket)
├── requirements.txt        # Dependências do projeto
├── database/
│   └── database.py         # Inicialização do banco e persistência de corridas
├── memory/
│   └── session_buffer.py   # Buffer em memória para sessões de corrida ativas
├── schemas/
│   └── telemetria.py       # Schemas Pydantic (validação dos pacotes)
├── websocket/
│   └── manager.py          # Gerenciador de conexões WebSocket (broadcast)
├── tests/
│   └── test_app.py         # Testes da API
└── db/
    └── telemetria.db       # Banco de dados SQLite (gerado automaticamente)
```

## Endpoints

### Telemetria e Dashboard
- **`POST /telemetria`**
  Recebe pacotes de telemetria do firmware. Valida o payload, atualiza a sessão em memória e, ao final da corrida, persiste o histórico no banco. Todo pacote válido é transmitido em tempo real via WebSocket.

- **`WS /ws/dashboard`**
  Endpoint WebSocket para conexão dos dashboards. Suporta múltiplos clientes simultâneos — cada pacote de telemetria válido recebido é transmitido a todos os conectados em tempo real.

### Histórico de Corridas
- **`GET /historico`**
  Retorna o histórico de corridas salvas. Aceita filtro opcional por tipo de labirinto (`4x4`, `8x8`, `16x16`).

- **`GET /historico/{corrida_id}`**
  Retorna os detalhes de uma corrida específica buscando pelo seu ID numérico.

- **`DELETE /historico`**
  Apaga todo o histórico de corridas do banco de dados. 

### Comandos (Front -> Robô)
- **`POST /comando`**
  Envia um comando de controle (ex: `start`, `reset`) ao robô. Se a serial gerida pelo backend estiver conectada, escreve na serial. Caso contrário, enfileira e tenta enviar pela ponte legada via WebSocket.

- **`POST /comandos`**
  Recebe um comando do Frontend e encaminha para o ESP32, verificando falhas de comunicação ou timeout.

- **`WS /ws/robo`**
  *(Legado)* Endpoint WebSocket onde a ponte bluetooth/serial externa pode se conectar para escutar comandos emitidos pelo frontend.

### Conexão Serial (Backend Bridge)
- **`GET /serial/portas`**
  Lista as portas seriais (COMs) disponíveis no host para conectar ao robô via Bluetooth.

- **`POST /serial/conectar`**
  Abre a conexão com uma porta serial especificada (ex: passando `{"port": "COM3"}` no corpo da requisição).

- **`POST /serial/desconectar`**
  Encerra a conexão serial ativa.

- **`GET /serial/status`**
  Retorna o status atual da conexão serial gerida pelo backend.

### Status e Monitoramento (Healthcheck)
- **`GET /health`**
  Retorna o status de saúde da API e a quantidade atual de clientes (dashboards) conectados via WebSocket.

- **`GET /esp32/status`**
  Retorna se o ESP32 está online baseando-se no tempo do último pacote de telemetria recebido (timeout de 5 segundos).

## Como executar

```bash
pip install -r requirements.txt
uvicorn app:app --reload
```

A API estará disponível em `http://localhost:8000`.  
Documentação interativa em `http://localhost:8000/docs`.

## Como testar o WebSocket

Com o servidor rodando, conecte um cliente WebSocket:

```bash
websocat ws://localhost:8000/ws/dashboard
```

Em outro terminal, envie um pacote de telemetria via `POST /telemetria` e o dashboard receberá o JSON imediatamente.