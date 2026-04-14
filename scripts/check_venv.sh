#!/bin/bash
VENV_DIR=".venv"

if ! command -v python3 &> /dev/null; then
    echo "[ERROR] Python3 nao encontrado no sistema. Instale-o antes de continuar."
    exit 1
fi

if [ ! -f "$VENV_DIR/bin/activate" ]; then
    echo "[INFO] Criando ambiente virtual em $VENV_DIR..."
    python3 -m venv "$VENV_DIR"
fi
