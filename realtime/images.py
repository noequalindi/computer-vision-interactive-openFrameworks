"""Herramienta generativa opcional; nunca reemplaza una imagen fallida por ondas."""
import base64
import gc
import io
from .download import MODELS

IMAGE_MODEL = "stabilityai/sd-turbo"
IMAGE_PATH = MODELS / "sd-turbo"


class ImageTool:
    def generate(self, prompt):
        if not (IMAGE_PATH / "model_index.json").is_file():
            raise RuntimeError("Falta SD-Turbo: ejecutar install_env --voice --images (Windows: -Voice -Images).")
        try:
            import torch
            from diffusers import AutoPipelineForText2Image
        except ImportError as exc:
            raise RuntimeError("Faltan dependencias de imagen: repetir install_env con --images.") from exc
        device = "cuda" if torch.cuda.is_available() else "mps" if torch.backends.mps.is_available() else "cpu"
        dtype = torch.float32 if device == "cpu" else torch.float16
        pipe = None
        try:
            # Pesos locales: no se descarga nada al conversar. CPU usa float32.
            pipe = AutoPipelineForText2Image.from_pretrained(str(IMAGE_PATH),
                torch_dtype=dtype, variant="fp16", local_files_only=True)
            pipe.to(device)
            pipe.enable_attention_slicing()
            with torch.inference_mode():
                result = pipe(prompt=prompt, num_inference_steps=2, guidance_scale=0.0,
                              width=512, height=512)
            if getattr(result, "nsfw_content_detected", None) and any(result.nsfw_content_detected):
                raise RuntimeError("El filtro del modelo no entregó una imagen para este pedido.")
            output = io.BytesIO()
            result.images[0].convert("RGB").save(output, format="JPEG", quality=88)
            return {"image_base64": base64.b64encode(output.getvalue()).decode(),
                    "image_model": IMAGE_MODEL, "image_device": device}
        finally:
            # Liberar antes del siguiente turno de LLM: no acumular ambos modelos.
            if pipe is not None:
                del pipe
            gc.collect()
            if device == "cuda":
                torch.cuda.empty_cache()
            elif device == "mps":
                torch.mps.empty_cache()


def download_images():
    from huggingface_hub import snapshot_download
    print("Descargando SD-Turbo fp16 (~2.6 GB de pesos); consultar licencia en su model card.", flush=True)
    snapshot_download(IMAGE_MODEL, local_dir=IMAGE_PATH, allow_patterns=[
        "model_index.json", "*/config.json", "scheduler/*.json", "tokenizer/*",
        "*/diffusion_pytorch_model.fp16.safetensors", "*/model.fp16.safetensors"])
