"""Descargas explícitas. Los modelos nunca se descargan durante una interacción."""
import argparse
from pathlib import Path
import urllib.request
import zipfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT / "models"
FILES = {
    "selfie_segmenter_landscape.tflite": "https://storage.googleapis.com/mediapipe-models/image_segmenter/selfie_segmenter_landscape/float16/1/selfie_segmenter_landscape.tflite",
    "pose_lite.task": "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task",
    "efficientdet_lite0.tflite": "https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/1/efficientdet_lite0.tflite",
}


def download(url, destination):
    if destination.exists():
        print("Ya existe:", destination.name)
        return
    temporary = destination.with_suffix(destination.suffix + ".part")
    try:
        print("Descargando", url, flush=True)
        urllib.request.urlretrieve(url, temporary)
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--voice", action="store_true")
    args = parser.parse_args()
    MODELS.mkdir(exist_ok=True)
    for name, url in FILES.items():
        download(url, MODELS / name)
    if args.voice:
        target = MODELS / "vosk-model-small-es-0.42"
        if not target.is_dir():
            archive = MODELS / "vosk-es.zip"
            download("https://alphacephei.com/vosk/models/vosk-model-small-es-0.42.zip", archive)
            # Extraer de forma transaccional: una interrupción no deja un modelo a medias.
            with tempfile.TemporaryDirectory(dir=MODELS) as folder:
                temporary = Path(folder)
                with zipfile.ZipFile(archive) as source:
                    for name in source.namelist():
                        if not (temporary / name).resolve().is_relative_to(temporary.resolve()):
                            raise ValueError("Ruta inválida en ZIP")
                    source.extractall(temporary)
                model = temporary / target.name
                if not (model / "am/final.mdl").is_file():
                    raise ValueError("Archivo de modelo incompleto")
                model.rename(target)
            archive.unlink()
    print("Pesos locales listos en", MODELS)


if __name__ == "__main__":
    main()
