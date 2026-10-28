import json
import time
from types import SimpleNamespace
import pytest
from fastapi.testclient import TestClient
from realtime import tts
from realtime.voice import Voice
from realtime.conversation import Conversation
from realtime.server import create_app


CATALOG = {"voices": [{"id": "Mónica", "name": "Mónica", "language": "es-ES"},
                      {"id": "Paulina", "name": "Paulina", "language": "es-MX"}],
           "default_voice": "Mónica", "notice": ""}


def test_mac_names_with_spaces_accents_and_locale():
    rows = tts.parse_mac('Mónica              es_ES    # Hola\nBad News            en_US    # Hi\n')
    assert rows == [{"id":"Mónica","name":"Mónica","language":"es-ES"},
                    {"id":"Bad News","name":"Bad News","language":"en-US"}]


def test_linux_voice_file_not_language_is_identifier():
    rows = tts.parse_linux('Pty Language Age/Gender VoiceName File Other Languages\n 5 es M Spanish_(Spain) roa/es (es 8)\n')
    assert rows[0]["id"] == "roa/es"
    assert rows[0]["language"] == "es"


def test_windows_single_voice_json_and_safe_stdin(monkeypatch):
    monkeypatch.setattr(tts.platform, "system", lambda: "Windows")
    monkeypatch.setattr(tts.subprocess, "run", lambda *a, **k: SimpleNamespace(stdout=json.dumps(CATALOG["voices"][0])))
    assert tts.installed_voices()["default_voice"] == "Mónica"
    text = 'Hola; $(Write-Host "x")'
    command, data = tts.speech_command(text, "Voice's name")
    assert text not in command[-1] and "Voice's name" not in command[-1]
    assert json.loads(data) == {"voice":"Voice's name", "text":text}


def test_spanish_first_and_refresh_cache(monkeypatch):
    monkeypatch.setattr(tts.platform, "system", lambda: "Darwin")
    monkeypatch.setattr(tts.subprocess, "run", lambda *a, **k: SimpleNamespace(stdout='Alex en_US # Hi\nPaulina es_MX # Hola\nMónica es_ES # Hola\n'))
    voice = Voice()
    assert voice.voices()["default_voice"] == "Mónica"
    assert voice.voices()["voices"][0]["language"].startswith("es")
    voice.validate_voice("Paulina")
    monkeypatch.setattr(tts.subprocess, "run", lambda *a, **k: SimpleNamespace(stdout='Alex en_US # Hi\n'))
    assert voice.voices()["default_voice"] == "Mónica"
    assert voice.voices(refresh=True)["default_voice"] == "Alex"
    with pytest.raises(ValueError): voice.validate_voice("Paulina")


def test_no_linux_engine_returns_empty_catalog(monkeypatch):
    monkeypatch.setattr(tts.platform, "system", lambda: "Linux")
    monkeypatch.setattr(tts.shutil, "which", lambda _: None)
    assert tts.installed_voices()["voices"] == []


def test_mac_text_is_stdin_not_command_options(monkeypatch):
    monkeypatch.setattr(tts.platform, "system", lambda: "Darwin")
    command, data = tts.speech_command('-o /tmp/not-a-file', 'Mónica')
    assert command == ['say','-v','Mónica']
    assert data.startswith('-o ')


class FakeVoice(Voice):
    def __init__(self):
        super().__init__()
        self.catalog = CATALOG
        self.spoken = []
    def speak(self, text, voice_id=""):
        self.validate_voice(voice_id)
        self.spoken.append((text, voice_id))
        return "ok"
    def voices(self, refresh=False):
        return CATALOG


class NoModels:
    def close(self): pass


class LLM(NoModels):
    def __init__(self): self.calls = 0
    def plan(self, history, text, images):
        self.calls += 1
        return {"reply":"Hola", "image_prompt":"", "color":[1,2,3], "energy":0.3}


def wait_done(client, job):
    for _ in range(200):
        data = client.post('/conversation/poll', json={'job':job,'session':'test'}).json()
        if data['state'] in ('done','failed'): return data
        time.sleep(.01)
    pytest.fail('Turno no terminó')


def test_catalog_preview_no_llm_history_and_turn_uses_selected_voice():
    voice, llm = FakeVoice(), LLM()
    conversation = Conversation(voice, llm)
    with TestClient(create_app(NoModels(), voice, conversation)) as client:
        assert client.post('/voices',json={}).json()['voices'] == CATALOG['voices']
        assert client.post('/voices',json={'refresh':'yes'}).status_code == 422
        assert client.post('/voice/preview',json={'session':'test','voice':'unavailable'}).status_code == 422
        assert client.post('/voice/preview',json={'session':'test','voice':['Mónica']}).status_code == 422
        job=client.post('/voice/preview',json={'session':'test','voice':'Paulina'}).json()['job']
        assert wait_done(client,job)['state'] == 'done'
        assert voice.spoken[-1][1] == 'Paulina'
        assert llm.calls == 0 and conversation.histories == {}
        job=client.post('/conversation/start',json={'session':'test','text':'hola','speak':True,'voice':'Mónica'}).json()['job']
        assert wait_done(client,job)['state'] == 'done'
        assert voice.spoken[-1] == ('Hola','Mónica')
        assert llm.calls == 1
        assert client.post('/voice/preview',json={},headers={'Origin':'https://example.org'}).status_code == 403


def test_failed_tts_process_is_reported():
    voice = Voice()
    voice.speaker = SimpleNamespace(poll=lambda: 1)
    with pytest.raises(RuntimeError, match="sintetizador"):
        voice.check_speech()
