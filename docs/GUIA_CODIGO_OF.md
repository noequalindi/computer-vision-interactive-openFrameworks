# Guía de código OF — probar, entender y modificar

Esta guía es para trabajar con los **cinco ejemplos**. Completá primero la
[instalación](../README.md#instalación-paso-a-paso). El objetivo es identificar
**entrada → respuesta del modelo → regla propia → salida visual o sonora**.

## Antes de editar

1. Abrí **un ejemplo a la vez** desde `OF/apps/myApps/<ejemplo>`.
2. Probá la versión original y confirmá que conecta. Para cámara, pulsá C; para voz,
   CONECTAR. Si pide carpeta, elegí el repositorio donde instalaste Python.
3. Editá el `src` del proyecto que compilás. La copia en este repo y la copia en OF
   son carpetas diferentes: guardar una no actualiza automáticamente la otra.
4. Cambiá una variante por vez, guardá, cerrá la app, recompilá y volvé a ejecutar.
5. Para revertir una variante, restaurá la línea original. No dejes dos declaraciones
   de la misma variable activas ni borres todo un bloque de control.

En los bloques «Elegir UNA línea», comentar la base y descomentar la alternativa:

```cpp
// Antes
const float expansion = openness;
// const float expansion = 1.f - openness;
```

```cpp
// Después
// const float expansion = openness;
const float expansion = 1.f - openness;
```

`//` convierte esa línea en comentario. Conservá llaves y puntos y coma.
Buscá las marcas **VARIANTE P1**, **O1**, **B1**, **BLOB1**, **V1**, etc., con Ctrl+F / ⌘F.

## El recorrido común

| Archivo / función | Qué hace | Qué podés cambiar |
| --- | --- | --- |
| `src/main.cpp` | Abre la ventana y entrega el ciclo a OF | Tamaño de ventana; también habría que adaptar el diseño si cambia. |
| `src/ofApp.h` | Declara funciones y guarda variables entre cuadros | Estado de tu propia interacción. |
| `setup()` | Prepara recursos y registra listeners | Fuente, color inicial, frecuencia de pedidos. |
| `update()` | Actualiza conexión y obra; envía pedidos cuando puede | Cómo evolucionan los valores con el tiempo. |
| `onResult()` | Recibe la respuesta en el hilo principal | Qué datos tomar y cómo transformarlos. |
| `draw()` | Dibuja el estado actual | Composición, tamaño, color, texto y materiales. |
| `exit()` | Libera recursos y quita listeners | Conservarlo al modificar un ejemplo. |

El registro `ofAddListener(..., this, &ofApp::onResult)` conecta el evento del addon
con nuestra función. OF llama `update()`/`draw()` continuamente; `onResult()` se
invoca cuando llega una respuesta, no necesariamente en cada cuadro.

En cámara se envía un **JPEG en JSON**, no un prompt de lenguaje. `kind` elige
`pose`, `objects` o `silhouette`. En voz se envía texto u orden de escuchar.

## poseLive

[Archivo principal](../poseLive/src/ofApp.cpp) · [Controles completos](../poseLive/README.md)

### Probar

- C: conectar; mostrar torso y brazos con buena luz.
- Extender brazos y sostener las esferas. P alterna física; Espacio agrega una; R
  limpia las actuales (seguirán apareciendo nuevas si la física está activa).
- S: activar sonido con volumen bajo y mover la muñeca derecha. Quietud implica
  menos volumen; S habilitado no significa sonido permanente.
- V oculta video y E oculta puntos; esas opciones no detienen la inferencia.
- Salir de cuadro y comprobar que no queda un cuerpo invisible sosteniendo esferas.

### Seguir el dato

1. `update()` → `vision.sendFrame("pose")` envía la imagen para estimar puntos.
2. `onResult()` → `landmarks = result["landmarks"]` guarda la pose recibida.
3. `visiblePoint()` comprueba visibilidad. Los índices 15/16 son las muñecas.
4. La diferencia entre posiciones dividida por tiempo se guarda en `motion`,
   limitado a 0..1. Es movimiento en imagen, no metros por segundo.
5. `physics.setPose(landmarks)` transforma brazos/torso en superficies físicas.
6. `tone.frequency` y `tone.volume` convierten movimiento en sonido;
   `physics.update()` y `physics.draw()` simulan y dibujan las esferas.

### Modificar

| Variante | Cambio | Resultado esperado |
| --- | --- | --- |
| P1 | Comentar `trackedPoint = 16` y descomentar 15 | Controlar el tono con la muñeca izquierda. |
| P2 | Usar la alternativa `650 - motion * 500` | Mayor movimiento produce un tono más grave. Activar S. |
| P3 | Descomentar el `ofDrawCircle(area.getCenter(), ...)` preparado | Un círculo crece según el movimiento. Es dibujo de OF. |
| P4 | Comentar `physics.setPose(landmarks);` y recompilar | El esqueleto sigue visible pero deja de sostener esferas. |

**Consigna:** elegí una parte del cuerpo y una consecuencia visual/sonora. Explicá
qué puntos estima el modelo y qué relación diseñaste vos.

## objectLive

[Archivo principal](../objectLive/src/ofApp.cpp) · [Controles](../objectLive/README.md)

### Probar

C: conectar. Mostrar taza y botella, moverlas, taparlas y retirarlas. Deben aparecer
cajas, categorías y scores. El ID es temporal: puede cambiar ante cruces/oclusiones.
Los anillos se activan sobre la taza por defecto. Este ejemplo no tiene audio.

### Seguir el dato

1. `sendFrame("objects")` pide detección de objetos.
2. `onResult()` guarda `result["objects"]`; una lista vacía es un resultado válido.
3. `draw()` recorre cada detección: `label`, `score`, `id`, `x`, `y`, `w`, `h`.
4. La caja viene normalizada. Por ejemplo, `area.x + x * area.width` lleva una
   posición 0..1 a píxeles de la instalación. Lo mismo se hace con y, ancho y alto.
5. `if (label == activeLabel)` decide qué objeto participa de la obra.
6. `ofDrawCircle()` dibuja anillos alrededor de su centro; el seno del tiempo los anima.

### Modificar

- **O1:** cambiar la línea activa `activeLabel = "cup"` por la alternativa `"bottle"`.
  El efecto debe seguir a una botella. Usar nombres exactos de categorías del modelo.
- **O2:** dentro de la condición anterior, comentar `ofNoFill(); // BASE` y
  descomentar `ofFill();`. Cambia de anillos a disco. No alterar el `ofNoFill()` de las cajas.

**Consigna:** hacé que un objeto cotidiano produzca un efecto inesperado.
El modelo reconoce la categoría; el significado que le das pertenece a tu obra.

## bodyPoem

[Archivo principal](../bodyPoem/src/ofApp.cpp) · [Shader](../bodyPoem/bin/data/shaders/poem.frag)

### Probar

- C: conectar; dejar hombros y muñecas visibles y abrir/cerrar los brazos.
- Observar cómo cambian tamaño, separación, color y textura de las letras.
- T cambia textura; P cambia paleta; S activa sonido.
- D permite explorar con mouse sin cámara ni ML. C vuelve al cuerpo.

### Seguir el dato

1. `sendFrame("pose")` reutiliza el mismo modelo que poseLive.
2. `onResult()` mide separación entre muñecas y la compara con separación entre
   hombros. `ofMap(..., 1, 3, 0, 1, true)` convierte esa relación en `targetOpenness`.
3. `update()` acerca suavemente `openness` al último valor recibido. Suavizar el
   dibujo no aumenta los FPS del modelo ni infiere poses nuevas.
4. `drawPoemMask()` coloca el texto en una imagen transparente en GPU (FBO).
5. `drawPoem()` envía apertura, tiempo, máscara y paleta al shader.
6. `poem.frag` pinta patrones sólo dentro de las letras. No son imágenes generadas
   por un modelo. Si falta el shader, se usa color plano como alternativa.

### Modificar

- **B1:** comentar `expansion = openness` y descomentar `1.f - openness`.
  Abrir brazos pasa a comprimir el poema; color y textura conservan la apertura real.
- **B2:** elegir la alternativa de `poem`, o escribir dos renglones propios separados
  por `\n`. Confirmar que el texto entra en pantalla.
- Después, cambiar las paletas `coldColors`/`warmColors` al inicio de `ofApp.cpp`.

**Consigna:** elegí si el poema acompaña o contradice el movimiento del cuerpo.
Compará ambas posibilidades con el mismo modelo y la misma entrada.

## blobPhysics

[Archivo principal](../blobPhysics/src/ofApp.cpp) · [Controles y límites](../blobPhysics/README.md)

### Probar

C: conectar; mostrar cabeza, torso y brazos. Moverse lentamente y sostener esferas.
V oculta video; M cambia relleno; L alterna lluvia; Espacio agrega una; R limpia.
D muestra una figura sintética que sigue al mouse. **El modo D no usa el modelo.**

### Seguir el dato

1. `sendFrame("silhouette")` pide segmentación de persona.
2. Python estima una máscara y obtiene sus contornos con OpenCV.
3. `onResult()` recorre `result["blobs"]` y `blob["contour"]`.
4. `outline.addVertex(...)` convierte puntos normalizados en píxeles de `stage`.
5. `physics.setContours(contours)` agrega superficies al mundo físico.
6. `physics.update(...)` resuelve caída/choques, y `draw()` representa figura y objetos.

### Modificar

**BLOB1:** comentar la llamada `physics.setContours(contours);` de **onResult()**,
recompilar y probar con C/cámara: el borde se ve, pero las esferas lo atraviesan.
Descomentar para recuperar colisiones. El modo D tiene otra llamada dentro de
`updateDemo()` y no demuestra esta variante del resultado del modelo.

M cambia sólo el relleno visual; el collider sigue activo. Es un buen contraste:
una cosa es lo que dibujás y otra la geometría que colisiona.

**Consigna:** usá el contorno como frontera, pincel o recorte. La máscara representa
persona/fondo, no identidad ni profundidad. Dos personas juntas pueden formar un
solo blob; en este ejemplo se usan contornos externos y no se preservan huecos.

## Cambiar la física de poseLive y blobPhysics

Abrí [ofxBodyPhysics.h](../ofxLocalAI/src/ofxBodyPhysics.h). En OF este archivo está
en `OF/addons/ofxLocalAI/src/`, compartido por ambos ejemplos.

| Lugar | Cambio para probar | Qué observar |
| --- | --- | --- |
| `setup()` | Cambiar gravedad vertical de 9.8 a 3.0 | Caída más lenta, sin cambiar la detección. |
| `spawn()` | Comparar `fixture.restitution = 0` con 0.8 | Menor/mayor rebote. |
| `draw()` | Sustituir el círculo principal por `ofDrawBitmapString("peso", position.x, position.y);` | Una palabra ocupa la posición del objeto. Su collider sigue siendo circular. |

El pequeño círculo de brillo en `draw()` es decorativo: también podés comentarlo
al cambiar la representación. Máximo 60 cuerpos. `reset()` limpia objetos;
`clearBody()` retira la superficie corporal. `resolveOverlaps()` saca objetos que
quedaron dentro de una figura actualizada: es una aproximación para interacción 2D.

## voiceScene

[Archivo principal](../voiceScene/src/ofApp.cpp) · [Instalación y voces](../voiceScene/README.md)

### Probar

1. Instalar con `--voice` / `-Voice`. Sin los pesos de SD-Turbo, apagar IMAGEN.
2. CONECTAR. Escribir «Hola, soy Ana» y luego «¿Cómo me llamo?» para probar contexto.
3. Con `--images` / `-Images` instalado e IMAGEN activo, pedir una ciudad musical.
4. ESCUCHAR: esperar LISTO y hablar. AUTO alterna escucha y respuesta; no es simultáneo.
5. Abrir el selector de voces, elegir un nombre y después PROBAR VOZ.
   Probar repite la selección; no cambia automáticamente al siguiente timbre.
6. NUEVA CHARLA empieza otra sesión cuando termina el turno actual.

### Seguir el dato

1. `send(bool microphone)` arma JSON con `session`, `text`, `microphone`, `speak`,
   `images` y `voice`. `send(false)` envía texto; `send(true)` solicita escucha en Python.
2. `/conversation/start` devuelve un `job`: identifica un trabajo en curso.
3. `update()` consulta `/conversation/poll` para ver progreso y salidas, sin reenviar
   el prompt en cada cuadro. `turnActive` evita turnos superpuestos.
4. `onResult()` recibe texto, color, energía y, si se generó, `image_base64`.
5. `ImageResponse.h` decodifica base64 y `ofImage.load()` carga el JPEG. No genera la imagen.
6. `draw()` muestra la respuesta y usa los parámetros para el marco/animación.

`session` identifica el historial; `job` identifica un turno. Cambiar sesión no
reentrena al modelo. La escritura progresiva depende de `reveal`: es una animación
de OF sobre texto recibido, no streaming de tokens.

### Modificar

- **V1:** comentar la asignación base de `energy` y descomentar `1.f - ...`.
  Ver el grosor del marco; la energía es un parámetro artístico, no una emoción medida.
- **V2:** comentar `float scale = 1.f;` y descomentar la alternativa sinusoidal.
  Una imagen generada empieza a respirar según energía. Si todavía no hay imagen,
  primero generar una para observar el efecto.
- Cambiar lo que hace `ofSetColor(color)` en el dibujo para que la respuesta afecte
  otro elemento de tu escena.

**Consigna:** elegí una salida —texto, color, energía o imagen— y dale una consecuencia
propia. Probá también un turno sin imagen y un pedido ambiguo.

Vosk transcribe, Qwen redacta, SD-Turbo genera píxeles y TTS pronuncia. Son herramientas
coordinadas. Qwen recibe texto; no mira la imagen ni escucha directamente el audio.

## Dónde está la conexión compartida

- [ofxLiveVision.h](../ofxLocalAI/src/ofxLiveVision.h), `sendFrame()`: toma la cámara,
  limita tamaño, aplica espejo y llama `request("/vision", ...)`.
- `received()` del mismo archivo: conserva el cuadro correspondiente al análisis
  y llama `ofNotifyEvent(...)`; así se ejecuta tu `onResult()`.
- [ofxLocalService.h](../ofxLocalAI/src/ofxLocalService.h): conecta/inicia Python local.
- [ofxTone.h](../ofxLocalAI/src/ofxTone.h), `audioOut()`: produce un tono sinusoidal
  en OF. No es el TTS de voiceScene; no agregar HTTP en el callback de audio.

Para inventar una experiencia, empezá modificando `ofApp.cpp`. No necesitás
reescribir el servidor, cambiar protocolos ni reentrenar modelos.

## Comprobar tu trabajo

- ¿Identificás qué entrada recibe el modelo y qué datos devuelve?
- ¿Podés señalar el bloque que transforma esos datos en una decisión de tu obra?
- ¿Funciona sin quedar un efecto activo cuando la persona/objeto desaparece?
- ¿La interfaz sigue respondiendo mientras espera una respuesta?
- ¿Podés explicar qué parte estimó el modelo y qué parte programaste vos?

Guardá una captura o video de tu variante y anotá el archivo, la regla que cambiaste
y el resultado. Evitá atribuir al modelo una decisión que tomaste en el código.
