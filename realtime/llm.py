"""LLM local: conversación y decisión de usar una herramienta de imagen."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import threading
from urllib.request import Request, ProxyHandler, build_opener
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[1]
MODEL = os.environ.get("VOICE_LLM_MODEL", "qwen2.5:1.5b-instruct")
URL = "http://127.0.0.1:11434"
SCHEMA = {
    "type": "object", "additionalProperties": False,
    "properties": {
        "reply": {"type": "string"},
        "image_prompt": {"type": "string"},
        "color": {"type": "array", "items": {"type": "integer", "minimum": 0, "maximum": 255}, "minItems": 3, "maxItems": 3},
        "energy": {"type": "number", "minimum": 0, "maximum": 1},
    }, "required": ["reply", "image_prompt", "color", "energy"],
}
SYSTEM = """Sos una instalación artística conversacional. Respondé en español natural,
con 1 a 3 frases breves, escuchando el contexto. Podés hablar de cualquier tema;
no tenés una lista cerrada de escenas ni comandos. No ejecutás código.
Devolvé únicamente JSON con reply, image_prompt, color [R,G,B] y energy de 0 a 1.
reply es lo que vas a decir al visitante. color y energy ambientan la interfaz.
image_prompt es una herramienta OPCIONAL: dejalo vacío en una charla normal.
Si te piden dibujar, crear o cambiar una imagen, escribí en image_prompt una
DESCRIPCIÓN VISUAL COMPLETA en inglés para Stable Diffusion, usando el historial.
No solo describas cómo dibujar: llená image_prompt cuando soliciten una imagen.
No prometas que una imagen ya está lista: la herramienta se ejecuta después.
Ejemplo de charla: usuario "Hola, soy Ana", salida
{"reply":"Hola, Ana. ¿Qué te gustaría explorar hoy?","image_prompt":"","color":[60,100,160],"energy":0.3}.
Ejemplo visual: usuario "Dibujá una ciudad de instrumentos", salida
{"reply":"Voy a imaginar esa ciudad. Podemos darle una atmósfera nocturna.","image_prompt":"A surreal city built from musical instruments, piano buildings, brass towers, dreamy night lighting, detailed illustration","color":[40,60,120],"energy":0.5}.
Contestá directamente la pregunta actual; no repitas saludos en cada turno.
image_prompt contiene SOLO la descripción en inglés, sin introducción en español.
La herramienta genera una nueva imagen; no puede ver, analizar ni editar píxeles
de imágenes previas. Puede reinterpretar su descripción. No ve la cámara.
"""


def request(path, payload=None, timeout=3):
    data = None if payload is None else json.dumps(payload).encode()
    req = Request(URL + path, data=data, headers={"Content-Type": "application/json"})
    # Un proxy del sistema nunca debe enviar la conversación local a Internet.
    try:
        with build_opener(ProxyHandler({})).open(req, timeout=timeout) as response:
            return json.load(response)
    except HTTPError as exc:
        detail = exc.read(1000).decode("utf-8", errors="replace")
        raise RuntimeError("Ollama: " + detail + ". Repetir install_env --voice si faltan pesos.") from exc


def find_ollama():
    candidates = [shutil.which("ollama"), "/opt/homebrew/bin/ollama", "/usr/local/bin/ollama",
                  "/Applications/Ollama.app/Contents/Resources/ollama",
                  Path.home() / "Applications/Ollama.app/Contents/Resources/ollama"]
    if os.environ.get("LOCALAPPDATA"):
        candidates.append(Path(os.environ["LOCALAPPDATA"]) / "Programs/Ollama/ollama.exe")
    return next((str(p) for p in candidates if p and Path(p).is_file()), None)


class LocalLLM:
    def __init__(self):
        self.process = None
        self.lock = threading.Lock()
        self.closed = False

    def ensure(self):
        if self.closed:
            raise RuntimeError("El servicio está cerrando.")
        try:
            if isinstance(request("/api/tags").get("models"), list):
                return
        except (OSError, ValueError, RuntimeError):
            pass
        binary = find_ollama()
        if not binary:
            raise RuntimeError("Falta Ollama: repetir install_env con --voice (Windows: -Voice).")
        with self.lock:
            if self.closed:
                raise RuntimeError("El servicio está cerrando.")
            if self.process is None or self.process.poll() is not None:
                folder = ROOT / ".local"
                folder.mkdir(exist_ok=True)
                with (folder / "ollama-voice.log").open("ab") as log:
                    self.process = subprocess.Popen([binary, "serve"], stdin=subprocess.DEVNULL,
                        stdout=log, stderr=log, env=dict(os.environ, OLLAMA_HOST=URL))
        for _ in range(40):
            if self.closed:
                raise RuntimeError("El servicio está cerrando.")
            try:
                if isinstance(request("/api/tags", timeout=1).get("models"), list):
                    return
            except (OSError, ValueError, RuntimeError):
                pass
            time.sleep(0.5)
        raise RuntimeError("Ollama no responde; revisar .local/ollama-voice.log.")

    def plan(self, history, text, images=True):
        self.ensure()
        instruction = SYSTEM + ("\nHerramienta de imagen habilitada." if images else
            "\nHerramienta de imagen desactivada: image_prompt debe quedar vacío. Si piden una imagen, explicá cómo activar IMAGEN en OF.")
        # Dos roles secuenciales sobre el MISMO modelo pequeño. Separar charla
        # y dirección visual evita que el formato JSON domine la conversación.
        chat_history = []
        for message in history:
            content = message["content"]
            if message["role"] == "assistant":
                saved = json.loads(content)
                content = saved["reply"]
                if saved.get("image_prompt"):
                    content += "\n[Descripción de imagen solicitada: " + saved["image_prompt"] + "]"
                if saved.get("image_error"):
                    content += "\n[La herramienta de imagen falló.]"
            chat_history.append({"role": message["role"], "content": content})
        conversation_system = (
            "Sos una instalación conversacional. Respondé directamente al último pedido "
            "en español, de forma cálida y breve (máximo 3 frases). Recordá lo dicho en "
            "el historial. No repitas saludos. Si preguntan su nombre, usá el que dijeron. "
            "Podés pedir a una herramienta local generar imágenes, pero no verlas. "
            "No digas que ya generaste una imagen: eso sucede después de tu respuesta. "
            "No generes código ni JSON en esta respuesta."
        )
        if not images:
            conversation_system += " La herramienta de imagen está apagada; para dibujar hay que activar IMAGEN."
        answer = request("/api/chat", {"model": MODEL,
            "messages": [{"role": "system", "content": conversation_system}, *chat_history,
                         {"role": "user", "content": text}],
            "stream": False, "keep_alive": "2m",
            "options": {"num_ctx": 4096, "num_predict": 220, "temperature": 0.5}}, timeout=240)
        reply = answer["message"]["content"].strip()
        if answer.get("done_reason") == "length":
            raise ValueError("El LLM llegó al límite de tokens; pedir una respuesta más breve.")
        if self.closed:
            raise RuntimeError("El servicio está cerrando.")
        messages = [{"role": "system", "content": instruction}, *history,
                    {"role": "user", "content": text}]
        response = request("/api/chat", {"model": MODEL, "messages": messages,
            "format": SCHEMA, "stream": False, "keep_alive": 0,
            "options": {"num_ctx": 4096, "num_predict": 600, "temperature": 0.2}}, timeout=240)
        if response.get("done_reason") == "length":
            raise ValueError("El director visual llegó al límite de tokens; simplificar el pedido.")
        plan = json.loads(response["message"]["content"])
        plan["reply"] = reply  # La voz lee la conversación, no la decisión del director.
        return validate_plan(plan)

    def close(self):
        # Nunca cerrar una app Ollama que ya estaba abierta antes de este servicio.
        with self.lock:
            self.closed = True
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


def validate_plan(plan):
    if not isinstance(plan, dict):
        raise ValueError("El LLM no devolvió un objeto JSON.")
    reply, prompt, color, energy = (plan.get(k) for k in ("reply", "image_prompt", "color", "energy"))
    if not isinstance(reply, str) or not reply.strip() or len(reply) > 2000:
        raise ValueError("Respuesta del LLM vacía o demasiado larga.")
    if not isinstance(prompt, str) or len(prompt) > 1200:
        raise ValueError("Prompt visual inválido.")
    if not isinstance(color, list) or len(color) != 3 or any(type(v) is not int or not 0 <= v <= 255 for v in color):
        raise ValueError("Color del LLM inválido.")
    if type(energy) not in (int, float) or not 0 <= energy <= 1:
        raise ValueError("Energía del LLM inválida.")
    return {"reply": reply.strip(), "image_prompt": prompt.strip(), "color": color, "energy": energy}
