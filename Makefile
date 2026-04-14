# Configuração de rede e caminhos
PORT = 8123
URL  = http://127.0.0.1:$(PORT)/
REQ  = requirements.txt

VENV        = .venv
SCRIPTS_DIR = scripts

# Detecção de Sistema Operacional (Windows vs Unix)
OS := $(shell (uname 2>NUL) || echo Windows_NT)

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
endif

.PHONY: help run setup build-up venv install verify serve clean

# help: Lista os principais comandos de automação
help:
	@$(CHCP_CMD) || true
	$(PRINT) "Comandos disponiveis no projeto LUI:"
	$(PRINT) "  make run      - Inicia o servidor MkDocs e abre o navegador automaticamente."
	$(PRINT) "  make setup    - Cria o ambiente virtual e instala as dependencias necessarias."
	$(PRINT) "  make clean    - Remove o ambiente virtual (.venv) e arquivos temporarios."
	$(PRINT) "  make serve    - Apenas inicia o servidor (sem abrir navegador ou verificar venv)."

# run: Atalho principal para desenvolvimento local
run:
	@$(CHCP_CMD) || true
	$(PRINT) "$(UI_BOLD)Iniciando servidor na porta $(PORT)...$(UI_RESET)"
	@$(OPEN_BROWSER)
	@$(MKDOCS) serve -a 127.0.0.1:$(PORT)

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