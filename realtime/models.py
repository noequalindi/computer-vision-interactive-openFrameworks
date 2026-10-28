"""Tres modelos preentrenados. OF recibe datos, no código generado."""
import time
import numpy as np
from .download import MODELS
from .tracking import CentroidTracker
from .silhouette import mask_to_blobs


class VisionModels:
    def __init__(self):
        self.pose = None
        self.segmenter = None
        self.detector = None
        self.tracker = CentroidTracker()
        self.session = None
        self.timestamp = 0

    def infer(self, image, kind, session):
        import mediapipe as mp
        # VIDEO permite reutilizar tracking de pose entre cuadros consecutivos.
        # Cambiar de cliente reinicia el estado; abrir un solo sketch a la vez.
        if self.session != session:
            self.close()
            self.session = session
            self.tracker = CentroidTracker()
        now = time.monotonic()
        self.timestamp = max(self.timestamp + 1, int(now * 1000))
        rgb = mp.Image(image_format=mp.ImageFormat.SRGB, data=np.asarray(image))
        base = mp.tasks.BaseOptions
        vision = mp.tasks.vision
        if kind == "pose":
            if self.pose is None:
                path = MODELS / "pose_lite.task"
                if not path.is_file():
                    raise ValueError("Faltan pesos: ejecutar install_env o python -m realtime.download")
                self.pose = vision.PoseLandmarker.create_from_options(vision.PoseLandmarkerOptions(
                    base_options=base(model_asset_path=str(path), delegate=base.Delegate.CPU),
                    running_mode=vision.RunningMode.VIDEO, num_poses=1,
                    min_pose_detection_confidence=0.5, min_tracking_confidence=0.5))
            result = self.pose.detect_for_video(rgb, self.timestamp)
            landmarks = []
            if result.pose_landmarks:
                landmarks = [{"x": p.x, "y": p.y, "z": p.z,
                              "visibility": p.visibility or 0.0} for p in result.pose_landmarks[0]]
            return {"model": "MediaPipe Pose Landmarker Lite", "landmarks": landmarks, "objects": []}
        if kind == "silhouette":
            if self.segmenter is None:
                path = MODELS / "selfie_segmenter_landscape.tflite"
                if not path.is_file():
                    raise ValueError("Falta silueta: ejecutar python -m realtime.download")
                self.segmenter = vision.ImageSegmenter.create_from_options(vision.ImageSegmenterOptions(
                    base_options=base(model_asset_path=str(path), delegate=base.Delegate.CPU),
                    running_mode=vision.RunningMode.VIDEO,
                    output_category_mask=False, output_confidence_masks=True))
            result = self.segmenter.segment_for_video(rgb, self.timestamp)
            # Este modelo tiene UN canal: probabilidad de persona.
            # Su category_mask usa 0/255 (no background=0/persona=1).
            # Umbralizar confianza evita invertir persona y fondo.
            person = result.confidence_masks[0].numpy_view() >= 0.5
            blobs = mask_to_blobs(person)
            return {"model": "MediaPipe Selfie Segmenter landscape", "landmarks": [],
                    "objects": [], "blobs": self.tracker.update(blobs, now)}
        if self.detector is None:
            path = MODELS / "efficientdet_lite0.tflite"
            if not path.is_file():
                raise ValueError("Faltan pesos: ejecutar install_env")
            self.detector = vision.ObjectDetector.create_from_options(vision.ObjectDetectorOptions(
                base_options=base(model_asset_path=str(path), delegate=base.Delegate.CPU),
                running_mode=vision.RunningMode.VIDEO, max_results=8, score_threshold=0.5))
        result = self.detector.detect_for_video(rgb, self.timestamp)
        width, height = image.size
        objects = []
        for detection in result.detections:
            box, category = detection.bounding_box, detection.categories[0]
            # Coordenadas normalizadas: la obra puede dibujar a cualquier resolución.
            x, y = max(0, box.origin_x / width), max(0, box.origin_y / height)
            right = min(1, (box.origin_x + box.width) / width)
            bottom = min(1, (box.origin_y + box.height) / height)
            objects.append({"label": category.category_name, "score": category.score,
                            "x": x, "y": y, "w": max(0, right - x), "h": max(0, bottom - y)})
        return {"model": "EfficientDet-Lite0 int8", "landmarks": [],
                "objects": self.tracker.update(objects, now)}

    def close(self):
        for model in (self.pose, self.detector, self.segmenter):
            if model is not None:
                model.close()
        self.pose = self.detector = self.segmenter = None
