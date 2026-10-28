# bodyPoem — El espacio respira

[Instalación completa](../README.md) · [Guía de código](../docs/GUIA_CODIGO_OF.md#bodypoem)

## Qué cambia con el cuerpo

El poema aparece en **Poppins ExtraBold grande, en dos líneas**. Al abrir los brazos:

- Aumentan el tamaño y la separación de las letras.
- El color recorre la paleta elegida.
- En textura automática, el relleno pasa de **grano → rayas → puntos**.
- Si el sonido está activado, sube la frecuencia del tono.

Pose Lite solo estima puntos del cuerpo. Las texturas, la tipografía y el sonido
son decisiones programadas en OF; no se descarga ningún modelo adicional.

## Cómo usarlo

1. Copiar el addon `ofxLocalAI` y esta carpeta **completa**, incluyendo `bin/data`, a OF.
2. Compilar y ejecutar. Pulsar **C / CONECTAR**, mostrar torso y muñecas, y abrir/cerrar brazos.
3. Para explorar solo el dibujo, pulsar **D** y mover el mouse sobre el poema,
   de izquierda a derecha. La interfaz indica **DEMO SIN MODELO**.

| Control | Acción |
| --- | --- |
| C / CONECTAR | Conectar a pose/cámara; también sale del modo demo. |
| T / botón de textura | Alternar automática, grano, rayas y puntos. |
| P / botón de paleta | Alternar océano/coral, lila/lima y fuego/rosa. |
| D / botón de demo | Probar con mouse sin inferencia; otra pulsación vuelve al cuerpo. |
| S / botón de sonido | Activar/desactivar sonido, inicialmente apagado. Empezar con volumen bajo. |

D permite probar el dibujo sin instalar Python ni abrir cámara. Si la cámara ya
estaba conectada, no se envían nuevas inferencias mientras la demo está activa.
Para que la pose funcione, sí hay que ejecutar el instalador del repositorio.

## Qué mostrar del código

En [src/ofApp.cpp](src/ofApp.cpp):

- `onResult()`: distancia entre muñecas / distancia entre hombros → apertura.
- `update()`: suaviza la apertura; D reemplaza la medida por la posición del mouse.
- `drawPoemMask()`: dibuja las letras en una imagen GPU transparente (**FBO**),
  ajusta separación y tamaño y limita la escala para no cortar el poema.
- `drawPoem()`: envía apertura, paleta y textura al **shader** (programa de dibujo GPU).
- [poem.frag](bin/data/shaders/poem.frag): el alfa de las letras recorta el relleno;
  después se calculan color, grano, rayas y puntos.
- `drawControls()`: cámara pequeña, apertura y botones con sus estados.

## Variantes para comentar/descomentar

Buscar `VARIANTE` en `drawPoemMask()`:

- **B1:** comentar `const float expansion = openness;` y descomentar
  `const float expansion = 1.f - openness;`. Abrir brazos comprime tamaño/separación;
  el color y la textura continúan respondiendo a la apertura real.
- **B2:** alternar las líneas de `poem` para mostrar `ME ABRO\nY ME CIERRO`.
  `\n` separa renglones. Se admiten acentos latinos y el texto se ajusta al espacio.

Dejar una sola alternativa activa, guardar, cerrar la app y recompilar.
Para cambiar las paletas, editar `coldColors` y `warmColors` al principio del archivo.
Para cambiar materiales, editar `poem.frag` y reiniciar la aplicación; no necesita recompilar C++.

## Actualizar una copia existente

Actualizar **`src/ofApp.cpp`, `src/ofApp.h` y `bin/data/fonts/`, `bin/data/shaders/`**.
No alcanza con copiar solo el C++. No hacen falta addons nuevos ni regenerar el proyecto.

```sh
make Release -j4
make RunRelease
```

El shader usa OpenGL 2.1 / GLSL 120, como la ventana original del ejemplo. Si falta
el shader, se muestra color plano y un aviso. Si falta Poppins, se intenta una
fuente del sistema. Conservar [la licencia de la fuente](bin/data/fonts/OFL.txt).
