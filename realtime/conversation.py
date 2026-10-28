"""Un coordinador LLM con herramientas; trabajos HTTP cortos para no bloquear OF.

No es un conjunto de agentes autónomos. Cada rol está separado para la clase:
ASR transcribe, LLM conversa/elige imagen, difusión dibuja, TTS pronuncia.
"""
from collections import OrderedDict
import json
import threading
import time
import uuid
from .llm import LocalLLM
from .images import ImageTool


class Conversation:
    def __init__(self, voice, llm=None, images=None):
        self.voice = voice
        self.llm = llm or LocalLLM()
        self.images = images or ImageTool()
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.worker = None
        self.jobs = OrderedDict()
        self.histories = OrderedDict()

    def start(self, data):
        session = data.get("session")
        if not isinstance(session, str) or not 1 <= len(session) <= 80:
            raise ValueError("session debe ser una cadena de 1 a 80 caracteres.")
        text = data.get("text", "")
        if not isinstance(text, str) or len(text) > 1200:
            raise ValueError("text debe ser texto de hasta 1200 caracteres.")
        for flag in ("microphone", "speak", "images", "preview"):
            if type(data.get(flag, False)) is not bool:
                raise ValueError(flag + " debe ser booleano.")
        if not data.get("preview", False) and not data.get("microphone", False) and not text.strip():
            raise ValueError("Escribir algo o activar el micrófono.")
        voice_id = data.get("voice", "")
        self.voice.validate_voice(voice_id)
        with self.lock:
            if self.worker and self.worker.is_alive():
                raise BlockingIOError("Hay un turno en curso; esperar a que termine.")
            if self.stop.is_set():
                raise RuntimeError("El servicio está cerrando.")
            job_id = uuid.uuid4().hex
            self.jobs[job_id] = {"job": job_id, "session": session, "state": "queued", "result": {}, "message": "Preparando turno..."}
            while len(self.jobs) > 8:
                self.jobs.popitem(last=False)
            self.worker = threading.Thread(target=self._run, args=(job_id, dict(data)), daemon=True)
            self.worker.start()
        return {"job": job_id, "state": "queued"}

    def poll(self, job_id, session):
        with self.lock:
            job = self.jobs.get(job_id)
            if job is None or job["session"] != session:
                raise KeyError("Turno no encontrado para esta sesión.")
            # No exponer el historial; copia consistente mientras escribe el worker.
            return {**job, "result": dict(job["result"])}

    def _publish(self, job_id, state, message, **values):
        with self.lock:
            job = self.jobs[job_id]
            job.update(state=state, message=message)
            job["result"].update(values)

    def _run(self, job_id, data):
        session = data["session"]
        try:
            if data.get("preview", False):
                self._publish(job_id, "speaking", "Probando la voz seleccionada...", preview=True, recognized=True)
                message = self.voice.speak("Hola. Esta es la voz elegida para nuestra conversación.", data.get("voice", ""))
                self._publish(job_id, "speaking", message)
                while self.voice.speaking() and not self.stop.wait(0.1):
                    pass
                if not self.stop.is_set():
                    self.voice.check_speech()
                self._publish(job_id, "done", "Prueba de voz terminada.")
                return
            if data.get("microphone", False):
                self._publish(job_id, "listening", "Hablá después de LISTO; una pausa termina tu turno.")
                text = self.voice.transcribe(self.stop, lambda message: self._publish(job_id, "listening", message))
            else:
                text = data["text"].strip()
            if self.stop.is_set():
                return
            if not text:
                self._publish(job_id, "done", "No se reconoció voz. Podés volver a escuchar o escribir.", text="", recognized=False)
                return
            self._publish(job_id, "thinking", "El LLM está preparando la respuesta...", text=text, recognized=True)
            history = self.histories.setdefault(session, [])
            self.histories.move_to_end(session)
            while len(self.histories) > 16:
                self.histories.popitem(last=False)
            plan = self.llm.plan(history, text, data.get("images", False))
            if not data.get("images", False):
                plan["image_prompt"] = ""  # El interruptor del alumno manda sobre la decisión del LLM.
            history.extend([{"role": "user", "content": text},
                            {"role": "assistant", "content": json.dumps(plan, ensure_ascii=False)}])
            del history[:-8]  # Cuatro intercambios recientes; memoria acotada por sesión.
            if self.stop.is_set():
                return
            self._publish(job_id, "reply", "Respuesta lista.", **plan)
            warning = ""
            if plan["image_prompt"]:
                self._publish(job_id, "drawing", "Stable Diffusion está generando una imagen...")
                try:
                    image = self.images.generate(plan["image_prompt"])
                    self._publish(job_id, "reply", "Imagen generada.", **image)
                except Exception as exc:
                    warning = "Imagen no generada: " + str(exc)[:400]
                    self._publish(job_id, "reply", warning, warning=warning)
                    # El siguiente turno debe saber que la herramienta falló.
                    history[-1]["content"] = json.dumps({**plan, "image_error": warning}, ensure_ascii=False)
            if self.stop.is_set():
                return
            if data.get("speak", False):
                self._publish(job_id, "speaking", "Pronunciando la respuesta...")
                try:
                    speech = self.voice.speak(plan["reply"] + (" No pude generar la imagen; revisá el aviso en pantalla." if warning else ""), data.get("voice", ""))
                    self._publish(job_id, "speaking", speech, speech_status=speech)
                    # AUTO no vuelve a abrir el micrófono mientras habla el sintetizador.
                    while self.voice.speaking() and not self.stop.wait(0.1):
                        pass
                    if not self.stop.is_set():
                        self.voice.check_speech()
                except Exception as exc:
                    warning += " Voz no disponible: " + str(exc)[:200]
            self._publish(job_id, "done", warning.strip() or "Tu turno: podés seguir la conversación.", warning=warning.strip())
        except Exception as exc:
            self._publish(job_id, "failed", str(exc)[:500])

    def close(self):
        self.stop.set()
        self.voice.close()
        self.llm.close()
        if self.worker:
            self.worker.join(timeout=2)
