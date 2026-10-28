"""Servidor local; un pedido en vuelo, sin cola de cuadros atrasados."""
import argparse
import base64
from contextlib import asynccontextmanager
import io
import json
import threading
import time
from PIL import Image, UnidentifiedImageError
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse
from starlette.concurrency import run_in_threadpool
from .models import VisionModels
from .voice import Voice
from .conversation import Conversation

MAX_BODY = 1_500_000
Image.MAX_IMAGE_PIXELS = 2_000_000


def decode_image(encoded):
    if not isinstance(encoded, str):
        raise ValueError("image debe ser base64")
    raw = base64.b64decode(encoded, validate=True)
    with Image.open(io.BytesIO(raw)) as image:
        if image.width > 1280 or image.height > 1280:
            raise ValueError("Reducir la imagen a 640 píxeles antes de enviar")
        return image.convert("RGB")


def create_app(models=None, voice=None, conversation=None):
    models, voice = models or VisionModels(), voice or Voice()
    vision_lock = threading.Lock()
    conversation = conversation or Conversation(voice)

    @asynccontextmanager
    async def lifespan(app):
        yield
        conversation.close()
        models.close()

    app = FastAPI(title="UNA / Computer vision on stage", lifespan=lifespan)

    @app.middleware("http")
    async def native_clients_only(request, call_next):
        # El cliente es OF. Una página web ajena no debe poder activar el micrófono.
        if request.headers.get("origin"):
            return JSONResponse({"detail": "Usar el cliente nativo local"}, status_code=403)
        return await call_next(request)

    async def payload(request):
        body = bytearray()
        async for chunk in request.stream():
            body.extend(chunk)
            if len(body) > MAX_BODY:
                raise HTTPException(413, "Solicitud demasiado grande")
        try:
            data = json.loads(body)
            if not isinstance(data, dict):
                raise ValueError()
            return data
        except (ValueError, UnicodeError):
            raise HTTPException(422, "Enviar un objeto JSON")

    @app.get("/health")
    def health():
        return {"service": "una-realtime", "protocol": 1, "device": "cpu", "voice_protocol": 3}

    @app.post("/vision")
    async def vision(request: Request):
        data = await payload(request)
        try:
            if data.get("kind") not in ("pose", "objects", "silhouette"):
                raise ValueError("kind: pose, objects o silhouette")
            if not isinstance(data.get("session"), str) or not 1 <= len(data["session"]) <= 80:
                raise ValueError("session inválida")
            if type(data.get("sequence")) is not int or data["sequence"] < 0:
                raise ValueError("sequence debe ser entero positivo o cero")
            image = decode_image(data.get("image"))
        except (ValueError, TypeError, OSError, UnidentifiedImageError, Image.DecompressionBombError) as exc:
            raise HTTPException(422, str(exc)) from exc
        if not vision_lock.acquire(blocking=False):
            raise HTTPException(429, "Modelo ocupado; descartar este frame")
        started = time.perf_counter()
        try:
            result = await run_in_threadpool(models.infer, image, data["kind"], data["session"])
            return {**result, "sequence": data["sequence"], "kind": data["kind"],
                    "inference_ms": round((time.perf_counter() - started) * 1000, 1)}
        except Exception as exc:
            raise HTTPException(503, str(exc)[:300]) from exc
        finally:
            vision_lock.release()

    @app.post("/voices")
    async def voices(request: Request):
        data = await payload(request)
        if type(data.get("refresh", False)) is not bool:
            raise HTTPException(422, "refresh debe ser booleano.")
        try:
            catalog = await run_in_threadpool(voice.voices, data.get("refresh", False))
            return {"kind": "voices", **catalog}
        except Exception as exc:
            raise HTTPException(503, "No se pudo consultar las voces: " + str(exc)[:300]) from exc

    @app.post("/voice/preview")
    async def voice_preview(request: Request):
        data = await payload(request)
        try:
            return await run_in_threadpool(conversation.start, {
                "session": data.get("session"), "voice": data.get("voice", ""), "preview": True})
        except ValueError as exc:
            raise HTTPException(422, str(exc)) from exc
        except BlockingIOError as exc:
            raise HTTPException(429, str(exc)) from exc

    @app.post("/conversation/start")
    async def conversation_start(request: Request):
        data = await payload(request)
        try:
            return await run_in_threadpool(conversation.start, data)
        except ValueError as exc:
            raise HTTPException(422, str(exc)) from exc
        except BlockingIOError as exc:
            raise HTTPException(429, str(exc)) from exc

    @app.post("/conversation/poll")
    async def conversation_poll(request: Request):
        data = await payload(request)
        if not isinstance(data.get("job"), str) or not isinstance(data.get("session"), str):
            raise HTTPException(422, "Enviar job y session como texto.")
        try:
            return conversation.poll(data["job"], data["session"])
        except KeyError as exc:
            raise HTTPException(404, "Turno no encontrado.") from exc

    @app.post("/listen")
    @app.post("/interpret")
    async def old_voice_api():
        raise HTTPException(410, "Actualizar voiceScene: usar /conversation/start y /conversation/poll.")

    return app


def main():
    import uvicorn
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8770)
    args = parser.parse_args()
    uvicorn.run(create_app(), host="127.0.0.1", port=args.port, access_log=False)


if __name__ == "__main__":
    main()
