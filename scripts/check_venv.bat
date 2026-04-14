@echo off
set "VENV_DIR=.venv"

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python nao encontrado no sistema. Instale-o antes de continuar.
    exit /b 1
)

if not exist "%VENV_DIR%\Scripts\activate.bat" (
    echo [INFO] Criando ambiente virtual em %VENV_DIR%...
    python -m venv %VENV_DIR%
)
