import base64
import io
import threading
import time
from concurrent.futures import ThreadPoolExecutor
import pytest
from fastapi.testclient import TestClient
from PIL import Image
from realtime.server import create_app
from realtime.tracking import CentroidTracker



class FakeModels:
    def infer(self, image, kind, session):
        return {"landmarks": [], "objects": [], "model": "fake"}

    def close(self):
        pass


class FakeVoice:
    def transcribe(self, stop, status):
        return "Hola, quiero conversar"

    def check_speech(self):
        pass

    def validate_voice(self, voice_id):
        return voice_id

    def speak(self, text, voice_id=""):
        return "fake"

    def speaking(self):
        return False

    def close(self):
        pass


@pytest.fixture
def client():
    with TestClient(create_app(FakeModels(), FakeVoice())) as client:
        yield client


def frame(**changes):
    buffer = io.BytesIO()
    Image.new("RGB", (32, 32)).save(buffer, format="JPEG")
    result = {"image": base64.b64encode(buffer.getvalue()).decode(),
              "session": "a", "kind": "pose", "sequence": 3}
    return {**result, **changes}


def test_empty_detection_is_valid_and_keeps_sequence(client):
    response = client.post("/vision", json=frame())
    assert response.status_code == 200
    assert response.json()["sequence"] == 3
    assert response.json()["landmarks"] == []


@pytest.mark.parametrize("change", [{"kind": "unknown"}, {"image": "bad"}, {"sequence": True},
                                    {"sequence": -1}, {"session": ""}, {"image": None}])
def test_bad_frames_do_not_reach_model(client, change):
    assert client.post("/vision", json=frame(**change)).status_code == 422


def test_body_limit(client):
    assert client.post("/vision", content=b"a" * 1_500_001).status_code == 413


def test_oversized_image(client):
    buffer = io.BytesIO()
    Image.new("RGB", (1300, 20)).save(buffer, format="PNG")
    assert client.post("/vision", json=frame(image=base64.b64encode(buffer.getvalue()).decode())).status_code == 422


def test_browser_cannot_start_microphone(client):
    assert client.post("/listen", json={}, headers={"Origin": "https://example.org"}).status_code == 403


def test_old_command_api_requires_updated_example(client):
    assert client.post("/interpret", json={"text": "mar"}).status_code == 410


def detection(x=0.1, label="cup"):
    return {"x": x, "y": 0.2, "w": 0.1, "h": 0.1, "label": label}


def test_tracker_no_duplicate_ids_and_expiration():
    tracker = CentroidTracker()
    original = tracker.update([detection()], 1)[0]["id"]
    result = tracker.update([detection(0.11), detection(0.12)], 1.1)
    assert result[0]["id"] == original
    assert len({d["id"] for d in result}) == 2
    assert tracker.update([detection()], 2)[0]["id"] != original


def test_tracker_respects_class():
    tracker = CentroidTracker()
    first = tracker.update([detection()], 1)[0]["id"]
    assert tracker.update([detection(label="bottle")], 1.1)[0]["id"] != first


def test_busy_model_rejects_instead_of_building_queue():
    entered, release = threading.Event(), threading.Event()

    class SlowModel(FakeModels):
        def infer(self, *args):
            entered.set()
            assert release.wait(5)
            return super().infer(*args)

    with TestClient(create_app(SlowModel(), FakeVoice())) as client, ThreadPoolExecutor() as pool:
        first = pool.submit(client.post, "/vision", json=frame())
        assert entered.wait(3)
        try:
            assert client.post("/vision", json=frame(sequence=4)).status_code == 429
        finally:
            release.set()
        assert first.result().status_code == 200


def test_silhouette_kind_reaches_adapter_and_preserves_empty_blobs():
    class SilhouetteModel(FakeModels):
        def infer(self, image, kind, session):
            assert kind == 'silhouette'
            return {**super().infer(image, kind, session), 'blobs': []}
    with TestClient(create_app(SilhouetteModel(), FakeVoice())) as client:
        response = client.post('/vision', json=frame(kind='silhouette', sequence=9))
        assert response.status_code == 200
        assert response.json()['blobs'] == []
        assert response.json()['sequence'] == 9
