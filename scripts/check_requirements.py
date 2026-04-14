import subprocess
import sys
import time
from typing import List

# Compatibilidade para metadados de pacotes (Python < 3.8)
try:
    import importlib.metadata as metadata
except ImportError:
    import importlib_metadata as metadata

# Bootstrap automático da biblioteca 'rich' para interface de terminal
try:
    from rich.progress import Progress, SpinnerColumn, BarColumn, TextColumn, TimeElapsedColumn, TaskProgressColumn
    from rich.console import Console
except ImportError:
    subprocess.run([sys.executable, "-m", "pip", "install", "-q", "rich"])
    from rich.progress import Progress, SpinnerColumn, BarColumn, TextColumn, TimeElapsedColumn, TaskProgressColumn
    from rich.console import Console

REQUIREMENTS_FILE = "requirements.txt"
CONSOLE = Console()

def get_required_packages(filename: str) -> List[str]:
    """Lê o arquivo requirements.txt e extrai o nome dos pacotes ignorando comentários."""
    try:
        with open(filename, "r") as f:
            return [line.strip() for line in f if line.strip() and not line.startswith("#")]
    except FileNotFoundError:
        CONSOLE.print(f"[bold red][ERROR] Arquivo de requisitos '{filename}' nao encontrado.[/bold red]")
        sys.exit(1)

def is_package_installed(package_name: str) -> bool:
    """Verifica se um pacote está presente no ambiente Python atual via importlib."""
    try:
        metadata.version(package_name)
        return True
    except metadata.PackageNotFoundError:
        return False

def install_package(package_name: str):
    """Executa a instalação silenciosa de um pacote via pip."""
    cmd = [sys.executable, "-m", "pip", "install", "-q", package_name]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        CONSOLE.print(f"\n[bold red][ERROR] Falha ao instalar {package_name}:[/bold red]")
        CONSOLE.print(result.stderr)
        sys.exit(1)

def main():
    """Lógica principal de sincronização de dependências."""
    packages = get_required_packages(REQUIREMENTS_FILE)
    to_install = [p for p in packages if not is_package_installed(p)]
    
    if not to_install:
        return

    CONSOLE.print(f"[bold cyan]Sincronizando dependencias ({len(to_install)} pendentes):[/bold cyan]")
    
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}", justify="left"),
        BarColumn(),
        TaskProgressColumn(),
        TimeElapsedColumn(),
    ) as progress:
        
        total_task = progress.add_task("[bold white]Progresso", total=len(to_install))
        
        for pkg in to_install:
            pkg_task = progress.add_task(f"Instalando {pkg:<15}", total=100)
            
            # Simulação visual de progresso para UX fluida
            for i in range(0, 101, 20):
                progress.update(pkg_task, advance=20)
                time.sleep(0.01)
            
            install_package(pkg)
            progress.update(total_task, advance=1)
            progress.remove_task(pkg_task)

    CONSOLE.print("[bold green]Dependencias sincronizadas.[/bold green]\n")

if __name__ == "__main__":
    main()
