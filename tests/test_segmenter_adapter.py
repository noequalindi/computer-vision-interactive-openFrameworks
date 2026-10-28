"""Regresión: Selfie Segmenter entrega una confianza, no categorías 0/1."""
import sys
from types import SimpleNamespace as NS
import numpy as np
from PIL import Image
from realtime.models import VisionModels


def test_adapter_thresholds_person_confidence(monkeypatch):
    probability = np.full((100, 100), .2, dtype=np.float32)
    probability[25:75, 25:75] = .9
    result = NS(confidence_masks=[NS(numpy_view=lambda: probability)])
    fake_mp = NS(Image=lambda **kwargs: kwargs, ImageFormat=NS(SRGB=1),
                 tasks=NS(BaseOptions=object, vision=NS()))
    monkeypatch.setitem(sys.modules, 'mediapipe', fake_mp)
    models = VisionModels()
    models.session = 'existing'
    models.segmenter = NS(segment_for_video=lambda *args: result)
    response = models.infer(Image.new('RGB', (100, 100)), 'silhouette', 'existing')
    assert len(response['blobs']) == 1
    blob = response['blobs'][0]
    assert .23 < blob['area'] < .26
    assert blob['x'] == .25
    probability.fill(.2)
    assert models.infer(Image.new('RGB', (100, 100)), 'silhouette', 'existing')['blobs'] == []
