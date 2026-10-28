"""Instala dependencias y pesos en el entorno seleccionado por install_env."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--voice", "-Voice", action="store_true", help="Agregar Vosk + conversación con Qwen en Ollama")
    parser.add_argument("--images", "-Images", action="store_true", help="Agregar SD-Turbo para voiceScene")
    args = parser.parse_args()
    args.voice = args.voice or args.images
    if sys.version_info[:2] != (3, 11):
        raise SystemExit("Usar Python 3.11: ejecutar install_env del sistema.")
    subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(ROOT / "requirements.txt")], check=True)
    if args.voice:
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(ROOT / "requirements-voice.txt")], check=True)
    if args.images:
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(ROOT / "requirements-images.txt")], check=True)
        from realtime.images import download_images
        download_images()
    if args.voice:
        from realtime.llm import LocalLLM, MODEL, request
        llm = LocalLLM()
        llm.ensure()
        # Ollama descarga y luego se libera: OF lo iniciará al conversar.
        print("Preparando " + MODEL + " en Ollama (~1 GB)...", flush=True)
        try:
            response = request("/api/pull", {"model": MODEL, "stream": False}, timeout=3600)
            if response.get("status") != "success":
                raise RuntimeError("Ollama no confirmó la descarga: " + str(response))
        finally:
            llm.close()
    command = [sys.executable, "-m", "realtime.download"]
    if args.voice:
        command.append("--voice")
    subprocess.run(command, cwd=ROOT, check=True)
    for example in ("poseLive", "objectLive", "bodyPoem", "voiceScene", "blobPhysics"):
        path = ROOT / example / "bin/data/local-service.json"
        path.write_text(json.dumps({"project_root": str(ROOT)}, ensure_ascii=False, indent=2), encoding="utf-8")
    print("Listo. Copiar addon y ejemplos a OF; CONECTAR inicia el servicio automáticamente.")
    print("Al mover esta carpeta, repetir instalación: los entornos contienen rutas absolutas.")


if __name__ == "__main__":
    main()
