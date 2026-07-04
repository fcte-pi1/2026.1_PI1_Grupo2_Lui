# Micromouse — Telemetria

Backend (FastAPI + WebSocket) e frontend (React) para recebimento, persistência (SQLite) e visualização em tempo real da telemetria do robô micromouse.

# Testes

## Pré-requisitos

* Python 3.10+
* Node.js 18+
* Make

## Instalação

```bash
make install-backend
make install-frontend
```

## Executando os testes

Você pode executar os testes a partir da raiz do repositório utilizando o `Makefile` ou diretamente em cada subdiretório.

### Backend (Pytest)

Os testes do backend utilizam o `pytest`. Para executá-los:

Usando o Make (na raiz do projeto):
```bash
make test-backend
```

Diretamente (em `src/backend`):
```bash
pytest
```

### Frontend Unitário (Jest)

Os testes unitários do frontend são feitos com o Jest. Para executá-los:

Usando o Make (na raiz do projeto):
```bash
make test-frontend
```

Diretamente (em `src/frontend`):
```bash
npm run test:unit
```

### E2E Frontend (Playwright)

Os testes End-to-End (ponta a ponta) do frontend utilizam o Playwright. Para executá-los:

Usando o Make (na raiz do projeto):
```bash
make test-frontend-e2e
```

Diretamente (em `src/frontend`):
```bash
npm run test:e2e
# Ou para abrir a interface do Playwright:
npx playwright test --ui
```

### Todos os testes (Backend + Frontend Unit)

Para rodar de forma automatizada tanto os testes do backend quanto do frontend:

```bash
make test
```

## Cobertura

```bash
make coverage-backend
make coverage-frontend
make test-coverage
```

Relatórios:

```text
src/backend/coverage_html/index.html
src/frontend/coverage/lcov-report/index.html
```

## Estrutura

```text
src/backend/tests/
├── test_app.py
├── test_known_walls.py
├── test_replay.py
└── test_source_success.py

src/frontend/src/
├── App.test.js
└── ...

src/frontend/tests/
└── *.spec.js
```

## Convenções

### Backend

```text
test_*.py
```

Exemplo:

```text
test_app.py
```

### Frontend (Jest)

```text
*.test.js
```

Exemplo:

```text
App.test.js
```

### Frontend (Playwright)

```text
*.spec.js
```

Exemplo:

```text
test_historico.spec.js
```

## Boas práticas

* Testes unitários próximos ao código ou em `tests/unit`.
* Testes de integração em `tests/integration`.
* Não misturar unitário e integração no mesmo arquivo.
* Utilizar mocks, fixtures e banco em memória.
* Nunca depender de dados reais.
* Garantir execução local e em CI.

## Comandos úteis

```bash
make test
make test-backend
make test-frontend
make test-frontend-e2e

make coverage-backend
make coverage-frontend
make test-coverage

make install-backend
make install-frontend
```
# Docker

A forma recomendada de executar o projeto é via Docker, eliminando a necessidade de instalar Python/Node localmente e garantindo o mesmo ambiente em qualquer máquina.

Há **dois modos** disponíveis, cada um com seu próprio arquivo de compose:

| Modo | Arquivo | Backend | Frontend |
|------|---------|---------|----------|
| **Desenvolvimento** (padrão) | `docker-compose.yml` | `uvicorn --reload`, código montado como volume | Servidor de dev do React (CRA), hot-reload, código montado como volume |
| **Produção** | `docker-compose.prod.yml` | `uvicorn` sem reload, código copiado na imagem | Build de produção do React servido por **Nginx** (imagem final enxuta, sem Node/`node_modules`) |

Os dois modos compartilham a mesma comunicação via WebSocket e a mesma persistência do SQLite em volume.

## Pré-requisitos

* [Docker](https://docs.docker.com/get-docker/) 24+
* [Docker Compose](https://docs.docker.com/compose/) v2 (já incluso no Docker Desktop / `docker compose`)

## Configuração

1. Copie o arquivo de variáveis de ambiente de exemplo (a partir da raiz do repositório):

   ```bash
   cp src/.env.example src/.env
   ```

   Esse passo também é feito automaticamente pelos alvos `make docker-*` caso o `src/.env` ainda não exista.

2. (Opcional) Ajuste as portas e URLs em `src/.env` conforme necessário. Os valores padrão já funcionam para a maioria dos casos:

   | Variável               | Padrão                              | Descrição                                            |
   |-------------------------|--------------------------------------|-------------------------------------------------------|
   | `BACKEND_PORT`          | `8000`                              | Porta do host mapeada para a API/WebSocket do backend |
   | `DB_PATH`               | `db/telemetria.db`                  | Caminho do arquivo SQLite dentro do container         |
   | `FRONTEND_PORT`         | `3000`                              | Porta do host mapeada para o dashboard React          |
   | `REACT_APP_API_BASE`    | `http://localhost:8000`             | URL da API REST usada pelo frontend                   |
   | `REACT_APP_WS_URL`      | `ws://localhost:8000/ws/dashboard`  | URL do WebSocket (valor de referência; configurável também na tela de Configurações do dashboard) |

## Modo desenvolvimento (hot-reload)

Usando o Make (na raiz do projeto):
```bash
make docker-up
```

Diretamente (em `src/`):
```bash
docker compose up --build
```

O código de ambos os serviços é montado como volume e os servidores rodam com hot-reload — alterações em `src/backend` ou `src/frontend` refletem automaticamente, sem precisar reconstruir as imagens.

## Modo produção (build estático + Nginx)

Usando o Make (na raiz do projeto):
```bash
make docker-up-prod
```

Diretamente (em `src/`):
```bash
docker compose -f docker-compose.prod.yml up --build
```

Aqui o frontend é compilado (`npm run build`) dentro da imagem e servido como arquivos estáticos pelo Nginx — sem Node, sem hot-reload, imagem final enxuta. O backend roda sem `--reload`. Use este modo para validar o comportamento "como em produção" antes de um deploy real.

## Acessando a aplicação

Em qualquer um dos dois modos:

* **Backend**: http://localhost:8000 — documentação interativa em http://localhost:8000/docs
* **WebSocket**: ws://localhost:8000/ws/dashboard
* **Frontend (dashboard)**: http://localhost:3000

## Persistência do banco SQLite

O arquivo `db/telemetria.db` do backend é gravado em um **volume nomeado** (`backend_db`), garantindo que o histórico de corridas **não seja perdido** ao recriar/reiniciar os containers (`make docker-down` + `make docker-up` mantém os dados). Para apagar o histórico junto com os containers, use `make docker-clean` (dev) / `make docker-clean-prod` (produção).

## Outros comandos úteis

**Via Make (na raiz do projeto):**

```bash
# Desenvolvimento
make docker-up          # sobe em modo desenvolvimento (foreground)
make docker-up-build    # sobe em background
make docker-down        # encerra os containers (mantém os dados)
make docker-logs        # acompanha os logs
make docker-ps          # lista os containers
make docker-restart     # reinicia os serviços
make docker-clean       # encerra e remove containers + volumes (apaga os dados)

# Produção
make docker-up-prod         # sobe em modo produção (foreground)
make docker-up-prod-build   # sobe em background
make docker-down-prod       # encerra os containers (mantém os dados)
make docker-logs-prod       # acompanha os logs
make docker-ps-prod         # lista os containers
make docker-clean-prod      # encerra e remove containers + volumes (apaga os dados)
```

**Diretamente (em `src/`):**

```bash
# Desenvolvimento
docker compose up --build -d   # sobe em background (detached)
docker compose logs -f         # acompanha os logs de backend e frontend
docker compose ps              # lista os containers e seus status
docker compose down            # encerra os containers (mantém os volumes/dados)
docker compose down -v         # encerra e remove TAMBÉM os volumes (apaga o histórico salvo)

# Produção (mesmos comandos, apontando para docker-compose.prod.yml)
docker compose -f docker-compose.prod.yml up --build -d
docker compose -f docker-compose.prod.yml logs -f
docker compose -f docker-compose.prod.yml ps
docker compose -f docker-compose.prod.yml down
docker compose -f docker-compose.prod.yml down -v
```

## Estrutura dos arquivos de containerização

```text
src/
├── docker-compose.yml         # orquestra backend + frontend (modo desenvolvimento)
├── docker-compose.prod.yml    # orquestra backend + frontend (modo produção)
├── .env.example                # variáveis de ambiente (copiar para .env)
├── backend/
│   ├── Dockerfile              # imagem Python/FastAPI
│   └── .dockerignore
└── frontend/
    ├── Dockerfile              # imagem Node/React (multi-stage: development + production)
    ├── nginx.conf               # config do Nginx usada no alvo "production"
    └── .dockerignore
```
