# objectLive — Objetos cotidianos

[Instalación completa](../README.md) · [Guía de código](../docs/GUIA_CODIGO_OF.md#objectlive)

Copiar `ofxLocalAI` a `OF/addons` y **esta carpeta completa** a `OF/apps/myApps`.
Importar el proyecto existente con Project Generator, seleccionar `ofxLocalAI`
y compilar. Primero ejecutar el instalador del repositorio. No necesita otros addons.

## Uso

CONECTAR abre webcam y detector. Mostrar una taza o botella.

## Código para modificar

Abrir [src/ofApp.cpp](src/ofApp.cpp). `onResult()` recibe cajas y etiquetas. `draw()` usa la taza como centro de anillos. IDs por cercanía, no identidad.

Cambiar C++ requiere recompilar la copia dentro de OF. Ejecutar un ejemplo a la vez.
La primera conexión puede pedir la carpeta del repositorio instalado.

## Variantes listas para probar

Buscar `VARIANTE` en `src/ofApp.cpp`: **O1 (taza o botella) y O2 (anillos o disco)**.
Comentar la línea base y descomentar la alternativa del mismo bloque: dejar
solo una activa. Guardar, cerrar la app y recompilar para ver el cambio.
La [guía de código](../docs/GUIA_CODIGO_OF.md#objectlive) indica qué mostrar y qué esperar de cada variante.
