

## Como executar a documentação localmente

### 1. Clone este repositório

```bash
git clone https://github.com/fcte-pi1/2026.1_PI1_Grupo2_Lui.git
```

### 2. Pré-requisitos

#### Linux (Ubuntu/Debian)

- Certifique-se de ter o `make`, `python3`, `pip` e `venv` instalados:

```bash
sudo apt update
sudo apt install -y make python3 python3-pip python3-venv
```

#### Windows

- Instale o [Git for Windows](https://git-scm.com/download/win)
- Instale o [Chocolatey](https://chocolatey.org/install) (executando o terminal **como administrador**), ou use o `Makefile` que instala automaticamente.
- Em seguida, instale o Make com:

```cmd
choco install make
```

### 3. Construir e iniciar os serviços com Make

Após instalar os pré-requisitos, execute:

```bash
make setup
make run
```

O script irá:

- Criar o ambiente virtual
- Instalar dependências
- Iniciar o servidor local com MkDocs

### 4. Acesse a documentação no navegador

Abra o navegador e vá para:

[http://127.0.0.1:8123](http://127.0.0.1:8123)
