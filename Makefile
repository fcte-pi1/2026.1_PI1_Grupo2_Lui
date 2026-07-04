# Configuração de rede e caminhos
PORT = 8123
URL  = http://127.0.0.1:$(PORT)/
REQ  = requirements.txt

VENV        = .venv
SCRIPTS_DIR = scripts
COMPOSE_DIR = src

# Caminhos das stacks de teste — separador correto por OS para o `cd` do cmd.
ifeq ($(OS),Windows_NT)
BACKEND_DIR  = src\backend
FRONTEND_DIR = src\frontend
else
BACKEND_DIR  = src/backend
FRONTEND_DIR = src/frontend
endif

# Detecção de Sistema Operacional (Windows vs Unix).
# No Windows, a variável de ambiente OS já vale "Windows_NT" e é importada
# pelo make; no Unix ela fica vazia e usamos `uname`. Não redirecionamos para
# NUL: o $(shell) roda em /bin/sh e, no Unix, "2>NUL" criaria um arquivo NUL.
ifneq ($(OS),Windows_NT)
OS := $(shell uname)
endif

# Mapeamento de binários e comandos específicos por OS
ifeq ($(OS),Windows_NT)
    PYTHON      = python
    PYTHON_VENV = $(VENV)\Scripts\python.exe
    PIP         = $(VENV)\Scripts\pip.exe
    MKDOCS      = $(VENV)\Scripts\mkdocs.exe
    CHECK_VENV  = call $(SCRIPTS_DIR)\check_venv.bat
    CHECK_REQ   = $(PYTHON_VENV) $(SCRIPTS_DIR)\check_requirements.py
    OPEN_BROWSER= cmd /c start $(URL)
    CHCP_CMD    = chcp 65001 >NUL
    DEVNULL     = NUL
    UI_BOLD     =
    UI_RESET    =
    PRINT       = @echo
    PAUSE_CMD   = pause
    # Caminho do Python do .venv visto a partir de src\backend\
    PY_FROM_BACKEND = ..\..\$(VENV)\Scripts\python.exe
else
    PYTHON      = python3
    PYTHON_VENV = $(VENV)/bin/python3
    PIP         = $(VENV)/bin/pip
    MKDOCS      = $(VENV)/bin/mkdocs
    CHECK_VENV  = sh $(SCRIPTS_DIR)/check_venv.sh
    CHECK_REQ   = $(PYTHON_VENV) $(SCRIPTS_DIR)/check_requirements.py
    OPEN_BROWSER= xdg-open $(URL)
    CHCP_CMD    = true
    DEVNULL     = /dev/null
    UI_BOLD     = \033[1;36m
    UI_RESET    = \033[0m
    PRINT       = @printf '%b\n'
    PAUSE_CMD   = true
    # Caminho do Python do .venv visto a partir de src/backend/
    PY_FROM_BACKEND = ../../$(VENV)/bin/python3
endif

.PHONY: help run run-backend run-frontend setup build-up venv install verify serve clean test test-backend test-frontend test-frontend-unit test-frontend-e2e test-coverage coverage-backend coverage-frontend install-backend install-frontend install-deps docker-env docker-up docker-up-build docker-down docker-build docker-logs docker-ps docker-restart docker-clean docker-up-prod docker-up-prod-build docker-down-prod docker-build-prod docker-logs-prod docker-ps-prod docker-clean-prod

# help: Lista os principais comandos de automação
help:
	@$(CHCP_CMD) || true
	$(PRINT) "Comandos disponiveis no projeto LUI:"
	$(PRINT) "  make run      - Inicia o servidor MkDocs e abre o navegador automaticamente."
	$(PRINT) "  make run-backend - Sobe a API backend (FastAPI/uvicorn) em http://127.0.0.1:8000."
	$(PRINT) "  make run-frontend- Sobe o dev server do frontend (CRA) em http://localhost:3000."
	$(PRINT) "  make setup    - Cria o ambiente virtual e instala as dependencias necessarias."
	$(PRINT) "  make clean    - Remove o ambiente virtual (.venv) e arquivos temporarios."
	$(PRINT) "  make serve    - Apenas inicia o servidor (sem abrir navegador ou verificar venv)."
	$(PRINT) ""
	$(PRINT) "Testes:"
	$(PRINT) "  make test              - Roda TODOS os testes (backend + frontend unit)."
	$(PRINT) "  make test-backend      - Roda os testes do backend (pytest)."
	$(PRINT) "  make test-frontend     - Roda os testes unitarios do frontend (Jest)."
	$(PRINT) "  make test-frontend-unit- Idem test-frontend (alias)."
	$(PRINT) "  make test-frontend-e2e - Roda os testes E2E do frontend (Playwright)."
	$(PRINT) "  make test-coverage     - Roda backend+frontend com relatorio HTML de cobertura."
	$(PRINT) "  make coverage-backend  - Cobertura do backend (HTML em src/backend/coverage_html/)."
	$(PRINT) "  make coverage-frontend - Cobertura do frontend (HTML em src/frontend/coverage/)."
	$(PRINT) ""
	$(PRINT) "Instalacao de dependencias das stacks:"
	$(PRINT) "  make install-deps      - Instala dependencias do backend E do frontend."
	$(PRINT) "  make install-backend   - Instala dependencias Python do backend."
	$(PRINT) "  make install-frontend  - Instala dependencias Node do frontend."
	$(PRINT) ""
	$(PRINT) "Docker (desenvolvimento - hot-reload):"
	$(PRINT) "  make docker-up         - Builda e sobe backend+frontend (foreground)."
	$(PRINT) "  make docker-up-build   - Idem, em background (detached)."
	$(PRINT) "  make docker-down       - Encerra os containers (mantem os volumes/dados)."
	$(PRINT) "  make docker-logs       - Acompanha os logs dos containers."
	$(PRINT) "  make docker-ps         - Lista os containers e seus status."
	$(PRINT) "  make docker-restart    - Reinicia os servicos."
	$(PRINT) "  make docker-clean      - Remove containers + volumes (apaga o historico salvo)."
	$(PRINT) ""
	$(PRINT) "Docker (producao - build estatico + Nginx):"
	$(PRINT) "  make docker-up-prod       - Builda e sobe backend+frontend (foreground)."
	$(PRINT) "  make docker-up-prod-build - Idem, em background (detached)."
	$(PRINT) "  make docker-down-prod     - Encerra os containers (mantem os volumes/dados)."
	$(PRINT) "  make docker-logs-prod     - Acompanha os logs dos containers."
	$(PRINT) "  make docker-ps-prod       - Lista os containers e seus status."
	$(PRINT) "  make docker-clean-prod    - Remove containers + volumes (apaga o historico salvo)."

# run: Atalho principal para desenvolvimento local
run:
	@$(CHCP_CMD) || true
	$(PRINT) "$(UI_BOLD)Iniciando servidor na porta $(PORT)...$(UI_RESET)"
	@$(OPEN_BROWSER)
	@$(MKDOCS) serve -a 127.0.0.1:$(PORT)

run-backend:
	@$(CHCP_CMD) || true
	$(PRINT) "$(UI_BOLD)Iniciando backend (FastAPI/uvicorn) em http://127.0.0.1:8000 ...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_backend.py --reload

run-frontend:
	@$(CHCP_CMD) || true
	$(PRINT) "$(UI_BOLD)Iniciando frontend (CRA) em http://localhost:3000 ...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_frontend.py start

# setup: Fluxo completo de inicialização do ambiente
setup: build-up

build-up:
	@$(CHCP_CMD) || true
	$(PRINT) "$(UI_BOLD)Iniciando instalacao do ambiente...$(UI_RESET)"
	@$(MAKE) venv
	@$(MAKE) install
	@$(MAKE) verify
	$(PRINT) "$(UI_BOLD)Sucesso! Use 'make run' para iniciar o projeto.$(UI_RESET)"

# Targets auxiliares de instalação
venv:
	@$(PRINT) "[1/3] Verificando interpretador Python e Venv..."
	@$(CHECK_VENV)

install:
	@$(PRINT) "[2/3] Sincronizando dependencias Python..."
	@$(CHECK_REQ)

verify:
	@$(PRINT) "[3/3] Validando configuracao..."
	@$(PYTHON_VENV) --version >$(DEVNULL) 2>&1 || (echo "Erro no ambiente virtual." && exit 1)

# serve: Comando cru para execução do MkDocs
serve:
	@$(MKDOCS) serve -a 127.0.0.1:$(PORT)

# clean: Reseta o diretório de trabalho removendo o venv
clean:
	@$(PRINT) "Limpando ambiente..."
ifeq ($(OS),Windows_NT)
	@if exist $(VENV) rmdir /s /q $(VENV)
else
	@rm -rf $(VENV)
endif
	@$(PRINT) "Limpeza concluida."

# ──────────────────────────────────────────────────────────────────────
# Instalação de dependências por stack
# ──────────────────────────────────────────────────────────────────────

# install-deps: instala as dependencias das DUAS stacks (backend + frontend)
install-deps: install-backend install-frontend
	@$(PRINT) "$(UI_BOLD)Dependencias de backend e frontend instaladas.$(UI_RESET)"

# install-backend: instala dependências Python do backend dentro do .venv
install-backend: venv
	@$(PRINT) "$(UI_BOLD)Instalando dependencias do backend...$(UI_RESET)"
	@$(PIP) install -r $(BACKEND_DIR)/requirements.txt

# install-frontend: instala dependências Node do frontend
install-frontend:
	@$(PRINT) "$(UI_BOLD)Instalando dependencias do frontend...$(UI_RESET)"
	@cd $(FRONTEND_DIR) && npm install

# ──────────────────────────────────────────────────────────────────────
# Testes
# ──────────────────────────────────────────────────────────────────────

# test-backend: roda pytest no backend usando o Python do .venv
# Usamos um wrapper Python para evitar problemas de `cd path && cmd` no
# cmd.exe/PowerShell. O wrapper faz chdir para src/backend e roda pytest.
test-backend:
	@$(PRINT) "$(UI_BOLD)Rodando testes do backend (pytest)...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_backend_tests.py

# test-frontend: roda os testes unitários do frontend (Jest via CRA)
test-frontend: test-frontend-unit

test-frontend-unit:
	@$(PRINT) "$(UI_BOLD)Rodando testes unitarios do frontend (Jest)...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_frontend_tests.py test:unit

# test-frontend-e2e: roda os testes E2E do frontend (Playwright)
test-frontend-e2e:
	@$(PRINT) "$(UI_BOLD)Rodando testes E2E do frontend (Playwright)...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_frontend_tests.py test:e2e

# test: roda todos os testes do projeto (backend + frontend unit)
# Obs.: testes E2E (Playwright) não são incluídos por padrão pois exigem o
# servidor do frontend de pé. Rode `make test-frontend-e2e` separadamente.
test:
	-@$(MAKE) test-backend
	-@$(MAKE) test-frontend-unit
	@$(PRINT) "$(UI_BOLD)Todos os testes (backend + frontend unit) concluidos.$(UI_RESET)"
	@$(PAUSE_CMD)

# ──────────────────────────────────────────────────────────────────────
# Cobertura de testes (threshold 70%)
# ──────────────────────────────────────────────────────────────────────

# coverage-backend: pytest com pytest-cov; gera HTML em src/backend/coverage_html/
# (pytest.ini ja inclui --cov, --cov-report=html e --cov-fail-under=70)
coverage-backend:
	@$(PRINT) "$(UI_BOLD)Gerando cobertura do backend...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_backend_tests.py
	@$(PRINT) "Relatorio HTML: $(BACKEND_DIR)/coverage_html/index.html"

# coverage-frontend: Jest com --coverage; gera HTML em src/frontend/coverage/lcov-report/
coverage-frontend:
	@$(PRINT) "$(UI_BOLD)Gerando cobertura do frontend...$(UI_RESET)"
	@$(PYTHON_VENV) $(SCRIPTS_DIR)/run_frontend_tests.py test:coverage
	@$(PRINT) "Relatorio HTML: $(FRONTEND_DIR)/coverage/lcov-report/index.html"

# test-coverage: alvo unico que roda as duas suites com cobertura
test-coverage: coverage-backend coverage-frontend
	@$(PRINT) "$(UI_BOLD)Cobertura gerada para backend e frontend.$(UI_RESET)"
	@$(PRINT) "Backend : $(BACKEND_DIR)/coverage_html/index.html"
	@$(PRINT) "Frontend: $(FRONTEND_DIR)/coverage/lcov-report/index.html"

# ──────────────────────────────────────────────────────────────────────
# Docker - desenvolvimento (hot-reload)
# Orquestrado por src/docker-compose.yml: backend com uvicorn --reload e
# frontend no servidor de dev do CRA, ambos com o codigo montado como volume.
# ──────────────────────────────────────────────────────────────────────

# docker-env: cria o .env a partir do .env.example se ainda nao existir
docker-env:
ifeq ($(OS),Windows_NT)
	@cd $(COMPOSE_DIR) && if not exist .env copy .env.example .env >$(DEVNULL)
else
	@cd $(COMPOSE_DIR) && test -f .env || cp .env.example .env
endif

# docker-up: builda e sobe backend+frontend com um unico comando (foreground)
docker-up: docker-env
	@$(PRINT) "$(UI_BOLD)Subindo containers (modo desenvolvimento)...$(UI_RESET)"
	@cd $(COMPOSE_DIR) && docker compose up --build

# docker-up-build: idem, em background (detached)
docker-up-build: docker-env
	@$(PRINT) "$(UI_BOLD)Subindo containers em background (modo desenvolvimento)...$(UI_RESET)"
	@cd $(COMPOSE_DIR) && docker compose up --build -d

# docker-down: encerra os containers (mantem os volumes/dados)
docker-down:
	@cd $(COMPOSE_DIR) && docker compose down

# docker-build: apenas builda as imagens, sem subir os containers
docker-build: docker-env
	@cd $(COMPOSE_DIR) && docker compose build

# docker-logs: acompanha os logs dos servicos em tempo real
docker-logs:
	@cd $(COMPOSE_DIR) && docker compose logs -f

# docker-ps: lista os containers do projeto e seu status
docker-ps:
	@cd $(COMPOSE_DIR) && docker compose ps

# docker-restart: reinicia os servicos (util apos editar o .env, por exemplo)
docker-restart:
	@cd $(COMPOSE_DIR) && docker compose restart

# docker-clean: remove containers, redes e volumes (inclui o SQLite e o
# node_modules) - use com cuidado, isso apaga o historico de corridas salvo
docker-clean:
	@cd $(COMPOSE_DIR) && docker compose down -v --remove-orphans

# ──────────────────────────────────────────────────────────────────────
# Docker - producao (build estatico + Nginx)
# Orquestrado por src/docker-compose.prod.yml: backend sem --reload e
# frontend buildado e servido por Nginx (imagem final enxuta, sem Node).
# ──────────────────────────────────────────────────────────────────────

docker-up-prod: docker-env
	@$(PRINT) "$(UI_BOLD)Subindo containers (modo producao)...$(UI_RESET)"
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml up --build

docker-up-prod-build: docker-env
	@$(PRINT) "$(UI_BOLD)Subindo containers em background (modo producao)...$(UI_RESET)"
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml up --build -d

docker-down-prod:
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml down

docker-build-prod: docker-env
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml build

docker-logs-prod:
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml logs -f

docker-ps-prod:
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml ps

docker-clean-prod:
	@cd $(COMPOSE_DIR) && docker compose -f docker-compose.prod.yml down -v --remove-orphans