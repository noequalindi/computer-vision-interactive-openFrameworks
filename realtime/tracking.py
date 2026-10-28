"""Asociación temporal didáctica. Los IDs NO son identidad de personas."""
import math


class CentroidTracker:
    """Empareja centros cercanos de la misma clase; expira tras 0,7 segundos.

    No es otro modelo ML. Puede intercambiar IDs si objetos iguales se cruzan.
    Todos los pares se ordenan por distancia y cada objeto se usa una sola vez.
    """

    def __init__(self, radius=0.18, ttl=0.7):
        self.radius = radius
        self.ttl = ttl
        self.tracks = {}
        self.next_id = 1

    def update(self, detections, now):
        self.tracks = {i: t for i, t in self.tracks.items() if now - t[2] < self.ttl}
        centers = [(d["x"] + d["w"] / 2, d["y"] + d["h"] / 2) for d in detections]
        pairs = []
        for index, detection in enumerate(detections):
            for identity, (label, center, _) in self.tracks.items():
                distance = math.dist(centers[index], center)
                if label == detection["label"] and distance < self.radius:
                    pairs.append((distance, index, identity))
        assigned, used = {}, set()
        for _, index, identity in sorted(pairs):
            if index not in assigned and identity not in used:
                assigned[index] = identity
                used.add(identity)
        for index, detection in enumerate(detections):
            identity = assigned.get(index)
            if identity is None:
                identity = self.next_id
                self.next_id += 1
            detection["id"] = identity
            self.tracks[identity] = (detection["label"], centers[index], now)
        return detections
