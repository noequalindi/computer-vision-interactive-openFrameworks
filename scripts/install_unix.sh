#!/bin/bash
set -euo pipefail
kit_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$kit_dir/.tools/downloads"
export UV_UNMANAGED_INSTALL="$kit_dir/.tools/uv"
export UV_PYTHON_INSTALL_DIR="$kit_dir/.tools/python"
export UV_CACHE_DIR="$kit_dir/.tools/cache"
uv_bin="$kit_dir/.tools/uv/uv"
if [[ ! -x "$uv_bin" ]]; then
    curl --fail --show-error --location --retry 3 https://astral.sh/uv/install.sh -o "$kit_dir/.tools/downloads/install-uv.sh"
    sh "$kit_dir/.tools/downloads/install-uv.sh"
fi
if [[ ! -d "$kit_dir/.venv" ]]; then
    "$uv_bin" python install 3.11
    "$uv_bin" venv --python 3.11 --managed-python --seed "$kit_dir/.venv"
fi
for option in "$@"; do
    if [[ "$option" == --voice || "$option" == --images ]]; then
        if ! "$kit_dir/.venv/bin/python" -c 'import sys; sys.path.insert(0, sys.argv[1]); from realtime.llm import find_ollama; assert find_ollama()' "$kit_dir" 2>/dev/null; then
            echo "Instalando Ollama desde su distribuidor oficial; puede pedir permisos del sistema."
            curl --fail --show-error --location --retry 3 https://ollama.com/install.sh -o "$kit_dir/.tools/downloads/install-ollama.sh"
            sh "$kit_dir/.tools/downloads/install-ollama.sh"
        fi
        break
    fi
done
"$kit_dir/.venv/bin/python" "$kit_dir/setup_local.py" "$@"
