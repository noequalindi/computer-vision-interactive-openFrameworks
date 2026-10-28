import threading
import time
import pytest
from fastapi.testclient import TestClient
from realtime.conversation import Conversation
from realtime.llm import validate_plan
from realtime.server import create_app
from realtime.voice import make_recognizer


class Voice:
    def __init__(self):
        self.spoken = []

    def transcribe(self, stop, status):
        status("LISTO")
        return "Me llamo Ana y quiero conversar sobre música"

    def check_speech(self):
        pass

    def validate_voice(self, voice_id):
        return voice_id

    def speak(self, text, voice_id=""):
        self.spoken.append(text)
        return "ok"

    def speaking(self):
        return False

    def close(self):
        pass


class LLM:
    def __init__(self, prompt=""):
        self.prompt, self.calls = prompt, []

    def plan(self, history, text, images):
        self.calls.append(([dict(m) for m in history], text, images))
        return {"reply": "Hola Ana, ¿qué música te gusta?", "image_prompt": self.prompt,
                "color": [30, 60, 90], "energy": 0.4}

    def close(self):
        pass


class Images:
    def __init__(self):
        self.calls = []

    def generate(self, prompt):
        self.calls.append(prompt)
        return {"image_base64": "dGVzdA==", "image_model": "fake"}


class Models:
    def close(self):
        pass


def finish(engine, job, session="test"):
    for _ in range(200):
        result = engine.poll(job["job"], session)
        if result["state"] in ("done", "failed"):
            return result
        time.sleep(0.01)
    pytest.fail("Trabajo no terminó")


def test_conversation_open_text_history_and_separate_sessions():
    llm, voice, images = LLM(), Voice(), Images()
    engine = Conversation(voice, llm, images)
    try:
        first = finish(engine, engine.start({"session": "test", "microphone": True, "speak": True}))
        assert first["state"] == "done"
        assert "Ana" in llm.calls[0][1]
        assert voice.spoken == [first["result"]["reply"]]
        finish(engine, engine.start({"session": "test", "text": "¿Cómo me llamo?"}))
        assert len(llm.calls[1][0]) == 2
        assert "Ana" in llm.calls[1][0][0]["content"]
        finish(engine, engine.start({"session": "other", "text": "hola"}), "other")
        assert llm.calls[2][0] == []
        assert images.calls == []
    finally:
        engine.close()


def test_image_tool_only_if_llm_requests_and_user_enabled():
    llm, images = LLM("A city made of musical instruments"), Images()
    engine = Conversation(Voice(), llm, images)
    try:
        result = finish(engine, engine.start({"session": "test", "text": "Dibujá una ciudad", "images": True}))
        assert result["result"]["image_base64"] == "dGVzdA=="
        assert len(images.calls) == 1
        result = finish(engine, engine.start({"session": "test", "text": "otra", "images": False}))
        assert len(images.calls) == 1
        assert "image_base64" not in result["result"]
    finally:
        engine.close()


def test_image_failure_preserves_reply_and_informs_next_turn():
    class BrokenImages:
        def generate(self, prompt):
            raise RuntimeError("Faltan pesos")
    llm = LLM("A city")
    engine = Conversation(Voice(), llm, BrokenImages())
    try:
        result = finish(engine, engine.start({"session": "test", "text": "Dibujá", "images": True}))
        assert result["state"] == "done"
        assert "Faltan pesos" in result["result"]["warning"]
        assert result["result"]["reply"]
        assert "image_base64" not in result["result"]
        finish(engine, engine.start({"session": "test", "text": "¿qué pasó?"}))
        assert "image_error" in llm.calls[-1][0][-1]["content"]
    finally:
        engine.close()


def test_jobs_return_immediately_busy_rejected_and_session_protected():
    entered, release = threading.Event(), threading.Event()
    class SlowLLM(LLM):
        def plan(self, *args):
            entered.set()
            assert release.wait(5)
            return super().plan(*args)
    engine = Conversation(Voice(), SlowLLM(), Images())
    try:
        with TestClient(create_app(Models(), Voice(), engine)) as client:
            response = client.post("/conversation/start", json={"session": "test", "text": "hola"})
            assert response.status_code == 200 and entered.wait(2)
            job = response.json()["job"]
            assert client.post("/conversation/start", json={"session": "other", "text": "hola"}).status_code == 429
            assert client.post("/conversation/poll", json={"job": job, "session": "other"}).status_code == 404
            assert client.post("/conversation/poll", json={"job": [], "session": "test"}).status_code == 422
            assert client.post("/conversation/poll", json={"job": job, "session": "test"}).json()["state"] == "thinking"
            release.set()
            assert finish(engine, {"job": job})["state"] == "done"
    finally:
        release.set()
        engine.close()


@pytest.mark.parametrize("data", [{"session":"test", "text":""}, {"session":"test", "text":1},
    {"session":"test", "text":"hola", "images":"yes"}, {"session":[], "text":"hola"},
    {"session":"test", "text":"x"*1201}])
def test_invalid_turns_never_start_models(data):
    engine = Conversation(Voice(), LLM(), Images())
    with pytest.raises(ValueError):
        engine.start(data)
    assert engine.worker is None


def test_asr_does_not_receive_a_closed_grammar(monkeypatch):
    import sys
    from types import SimpleNamespace
    calls = []
    monkeypatch.setitem(sys.modules, "vosk", SimpleNamespace(KaldiRecognizer=lambda *args: calls.append(args)))
    make_recognizer("model")
    assert calls == [("model", 16000)]


@pytest.mark.parametrize("change", [{"reply":""}, {"color":[-1,1,2]}, {"energy":float('nan')}, {"image_prompt":2}])
def test_llm_contract_rejects_bad_data(change):
    with pytest.raises(ValueError):
        validate_plan({"reply":"hola", "image_prompt":"", "color":[1,2,3], "energy":0.2, **change})
