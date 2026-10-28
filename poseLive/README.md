# poseLive — esqueleto, física, movimiento y sonido

[Instalación general](../README.md) · [Código y variantes P1–P4](../docs/GUIA_CODIGO_OF.md#poselive)

## Cómo activar todo

1. Preparar Python con el instalador del repositorio; copiar el addon `ofxLocalAI`
   y este proyecto completo a OF; instalar además [ofxBox2d](https://github.com/vanderlin/ofxBox2d)
   en `OF/addons/ofxBox2d`. Seleccionar **ofxLocalAI + ofxBox2d**, importar con Project Generator y compilar.
2. Pulsar **C / CONECTAR**. Autorizar la cámara. Si pide una carpeta, seleccionar
   el repositorio donde se ejecutó el instalador. Esperar la primera carga.
3. Mostrar torso y manos. Los puntos dibujan la pose de una persona.
   Extender los brazos: son superficies donde las esferas pueden apoyarse.
   Moverlos despacio para empujar objetos; la física comienza activada.
4. Mover la **muñeca derecha**: cambia la barra de movimiento.
5. Pulsar **S / SONIDO** con volumen bajo. Mover la muñeca para escuchar;
   cuando está quieta, fuera de cuadro o sin datos recientes, el volumen es cero.

| Control | Qué hace |
| --- | --- |
| C / CONECTAR | Abre cámara y conecta/inicia el servicio Python local. |
| P / FISICA | Alterna simulación y dibujo de esferas. |
| R / REINICIAR | Quita esferas; la lluvia vuelve a crear nuevas. |
| Espacio | Agrega una esfera cuando la física está activa. |
| S / SONIDO | Activa/desactiva la salida de audio. El botón muestra el estado. |
| V / VIDEO | Muestra/oculta la cámara; no detiene la captura ni el modelo. |
| E / PUNTOS | Muestra/oculta el esqueleto; no detiene el modelo ni el sonido. |

Los controles también funcionan haciendo clic en sus botones. La ayuda muestra
movimiento, cantidad de esferas y mediciones de rendimiento.
Cerrar la ventana termina el ejemplo. Ejecutar un solo ejemplo a la vez.

## Qué mostrar del código

Abrir [src/ofApp.cpp](src/ofApp.cpp):

- `update()` llama `sendFrame("pose")`.
- `onResult()` recibe `landmarks` y calcula velocidad de la muñeca.
- `physics.setPose(landmarks)` en `onResult()` construye colliders en brazos/torso.
- `physics.update(...)` en `update()` avanza gravedad y colisiones a paso fijo.
- `ofxLocalAI/src/ofxBodyPhysics.h`: gravedad en `setup()`, material/radio en `spawn()`.
- `tone.frequency` y `tone.volume` transforman movimiento en sonido.
- `draw()` dibuja imagen/esqueleto; `drawInstructions()` dibuja la ayuda.
- `keyPressed()` y `mousePressed()` comparten las acciones de los controles.

Buscar **VARIANTE P1** (otra muñeca), **P2** (invertir tono), **P3** (círculo que crece).
En P1/P2 comentar la base y descomentar la alternativa; dejar una sola activa.
P3 se agrega descomentando una línea. [Paso a paso](../docs/GUIA_CODIGO_OF.md#poselive).

Guardar, cerrar la aplicación y ejecutar desde la copia en OF:

```sh
make Release -j4
make RunRelease
```

## Si no reacciona

- Sin cámara: revisar permisos y `bin/data/camera.json`.
- Sin puntos: buena luz, una persona, torso y manos visibles.
- Audio APAGADO después de S: revisar salida de audio y consola de OF.
- Audio ACTIVADO pero sin sonido: comprobar movimiento, visibilidad y volumen del sistema.
- Sin servicio: ejecutar el instalador y revisar la carpeta elegida. Logs en `.local/managed/` del repo.

## Física y límites

**P4:** comentar `physics.setPose(landmarks);` para comparar esferas con gravedad
pero sin interacción corporal. Recompilar para probar cada variante. La pose
es una estimación RGB de una persona; no mide fuerzas ni profundidad. Los
segmentos se reconstruyen al llegar cada respuesta y se corrigen solapamientos.
La física interpola el movimiento de los objetos, no inventa poses entre cuadros.
Si no hay datos recientes se quita el collider. Máximo 60 esferas.
Para chocar con toda la figura, usar [blobPhysics](../blobPhysics/README.md).
