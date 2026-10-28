# ofxLocalAI — copia completa para esta clase

Copiar esta carpeta a `OF/addons/ofxLocalAI` y seleccionar **ofxLocalAI** en Project
Generator. No agregar rutas de fuentes adicionales. Compatible con OF 0.12.1 / C++17.

Conserva `setup`, `chat`, `listModels`, `classifyImage`, `update`, `close` y los
eventos `response` / `error` de las clases LLM/ViT. Agrega:

- `request(path, json)`: solicitud POST JSON sin bloquear OF.
- `health()`: comprobar el servidor de esta clase.
- `ofxLocalService`: conectar/iniciar Python local, con propiedad del proceso.
- `ofxLiveVision`: captura, espejo, límite de envíos y asociación imagen/respuesta.
- `ofxVisionDraw`: dibujo de puntos y esqueleto, independiente del modelo.
- `ofxTone`: oscilador simple para sonificación, audio apagado al comenzar.

Los métodos públicos se llaman desde el hilo principal. `false` al enviar significa
ocupado/no iniciado; no se encola otra solicitud. Llamar `update()` cada frame.
La respuesta de `request()` llega como `{request: "custom", body: {...}}`.

El puerto local es 8770. El contrato está implementado en `realtime/server.py`.
Los ejemplos de cámara no necesitan Ollama. En voiceScene, Python coordina Qwen
en Ollama (11434), Vosk, SD-Turbo opcional y TTS. OF sigue conectado al puerto 8770
mediante /conversation/start y /conversation/poll. Para las APIs antiguas, usar la
URL del servidor correspondiente como en los ejemplos de esas clases.

Esta carpeta es una copia autónoma; actualizar el addon completo en OF antes de
compilar. Los otros repositorios conservan sus copias originales.

## Física corporal opcional

`ofxBodyPhysics.h` convierte puntos de pose o contornos de persona en colliders
y simula esferas con gravedad. Requiere añadir **ofxBox2d** al proyecto que lo
incluye (`poseLive` y `blobPhysics`). El resto de `ofxLocalAI` sigue funcionando
sin ese addon. `setPose()` / `setContours()` actualizan geometría, `update()`
avanza la simulación, `draw()` muestra los objetos; `clearBody()` retira una figura
perdida. [Guía de ejemplo](../blobPhysics/README.md).
