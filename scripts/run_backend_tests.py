import os
import subprocess
import sys

HERE = os.path.abspath(os.path.dirname(__file__))
BACKEND = os.path.normpath(os.path.join(HERE, "..", "src", "backend"))

if not os.path.isdir(BACKEND):
    print(f"[run_backend_tests] diretório não encontrado: {BACKEND}", file=sys.stderr)
    sys.exit(2)

os.chdir(BACKEND)
sys.exit(subprocess.call([sys.executable, "-m", "py"] + sys.argv[1:]))
test