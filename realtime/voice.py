"""ASR de vocabulario abierto y TTS del sistema; el diálogo lo genera el LLM."""
import json
import subprocess
import time
import threading
from .tts import installed_voices, speech_command
from .download import MODELS


def make_recognizer(model):
    from vosk import KaldiRecognizer
    # Sin lista de comandos: se reconoce lenguaje libre en español.
    return KaldiRecognizer(model, 16000)


class Voice:
    def __init__(self):
        self.model = None
        self.speaker = None
        self.catalog = None
        self.catalog_lock = threading.Lock()

    def transcribe(self, stop, status):
        import numpy as np
        import sounddevice as sd
        from vosk import Model
        if self.speaking():
            self.speaker.terminate()
            self.speaker.wait(timeout=3)
        if self.model is None:
            status("Cargando reconocimiento de voz...")
            path = MODELS / "vosk-model-small-es-0.42"
            if not path.is_dir():
                raise ValueError("Falta Vosk: ejecutar install_env --voice (Windows: -Voice).")
            self.model = Model(str(path))
        recognizer = make_recognizer(self.model)
        words = []
        last_sound = None
        started = time.monotonic()
        # 250 ms por bloque; pausa de 1.4 s termina el turno, máximo 25 s.
        # La detección de silencio es una regla acústica, no un segundo modelo.
        if stop.is_set():
            return ""
        with sd.RawInputStream(samplerate=16000, blocksize=4000, channels=1, dtype="int16") as stream:
            status("LISTO: te escucho. Hablá; una pausa de 1.4 s termina el turno.")
            while not stop.is_set():
                audio, overflow = stream.read(4000)
                if overflow:
                    status("Audio saturado; acercate al micrófono y probá una frase breve.")
                raw = bytes(audio)
                samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768
                level = float(np.sqrt(np.mean(samples * samples)))
                now = time.monotonic()
                if level > 0.012:
                    last_sound = now
                if recognizer.AcceptWaveform(raw):
                    words.append(json.loads(recognizer.Result()).get("text", ""))
                if last_sound is not None and now - last_sound > 1.4:
                    break
                if now - started > (25 if last_sound is not None else 8):
                    break
        words.append(json.loads(recognizer.FinalResult()).get("text", ""))
        return " ".join(word for word in words if word).strip()

    def speaking(self):
        return self.speaker is not None and self.speaker.poll() is None

    def voices(self, refresh=False):
        with self.catalog_lock:
            if self.catalog is None or refresh:
                self.catalog = installed_voices()
            return {**self.catalog, "voices": [dict(v) for v in self.catalog["voices"]]}

    def validate_voice(self, voice_id):
        if not isinstance(voice_id, str) or len(voice_id) > 200:
            raise ValueError("voice debe ser el identificador de una voz instalada.")
        if voice_id and voice_id not in {v["id"] for v in self.voices()["voices"]}:
            raise ValueError("La voz elegida ya no está disponible. Actualizar VOCES y elegir otra.")
        return voice_id

    def speak(self, text, voice_id=""):
        self.validate_voice(voice_id)
        selected = voice_id or self.voices()["default_voice"]
        if not selected:
            raise RuntimeError(self.voices()["notice"])
        if self.speaking():
            self.speaker.terminate()
            self.speaker.wait(timeout=3)
        command, payload = speech_command(text, selected)
        # El texto y la selección viajan como datos: sin shell ni interpolación
        # en PowerShell. También evita interpretar un texto que empieza con '-'.
        self.speaker = subprocess.Popen(command, stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True, encoding="utf-8")
        self.speaker.stdin.write(payload)
        self.speaker.stdin.close()
        return "Hablando con " + selected + "."

    def check_speech(self):
        if self.speaker is not None and self.speaker.poll() not in (None, 0):
            raise RuntimeError("El sintetizador no pudo usar esa voz. Actualizar VOCES o elegir otra.")

    def close(self):
        if self.speaker is not None and self.speaker.poll() is None:
            self.speaker.terminate()
