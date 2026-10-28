"""Máscara de persona -> blobs geométricos; OpenCV no entrena otro modelo."""
import cv2
import numpy as np


def mask_to_blobs(mask, min_area=0.015, max_blobs=4, max_points=96):
    """Contornos externos normalizados. Ignoramos ruido pequeño y agujeros internos.

    La máscara ya es binaria (persona=1). Reducimos a 320 px para limitar coste,
    suavizamos bordes y simplificamos antes de enviar puntos por HTTP.
    """
    if mask.ndim != 2 or not mask.size:
        raise ValueError("Se espera una máscara 2D no vacía")
    binary = (mask > 0).astype(np.uint8) * 255
    height, width = binary.shape
    scale = min(1.0, 320 / max(width, height))
    if scale < 1:
        binary = cv2.resize(binary, (max(1, round(width * scale)),
                                    max(1, round(height * scale))), interpolation=cv2.INTER_NEAREST)
    binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
    height, width = binary.shape
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    blobs = []
    for contour in sorted(contours, key=cv2.contourArea, reverse=True):
        area = cv2.contourArea(contour) / (width * height)
        if area < min_area:
            continue
        # Aumentar epsilon reduce puntos sin saltar arbitrariamente vértices.
        epsilon = 1.5
        simplified = cv2.approxPolyDP(contour, epsilon, True)
        while len(simplified) > max_points:
            epsilon *= 1.4
            simplified = cv2.approxPolyDP(contour, epsilon, True)
        if len(simplified) < 3:
            continue
        x, y, w, h = cv2.boundingRect(contour)
        moments = cv2.moments(contour)
        blobs.append({
            "label": "person", "x": x / width, "y": y / height,
            "w": w / width, "h": h / height, "area": area,
            "cx": moments["m10"] / moments["m00"] / width,
            "cy": moments["m01"] / moments["m00"] / height,
            "contour": [{"x": float(p[0][0]) / width, "y": float(p[0][1]) / height}
                        for p in simplified],
        })
        if len(blobs) >= max_blobs:
            break
    return blobs
