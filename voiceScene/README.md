# voiceScene — conversación, voz e imágenes generadas

La persona habla o escribe libremente. Un **LLM local conserva el contexto** y
responde. Si le pedís dibujar, el LLM prepara un prompt y llama a **Stable Diffusion**.
La respuesta también se pronuncia con la voz del sistema operativo.

## Modelos y responsabilidades

| Componente | Tipo | Trabajo |
| --- | --- | --- |
| Vosk small español 0.42 | ASR / reconocimiento de voz | Audio del micrófono → texto libre; sin gramática de cinco palabras |
| Qwen2.5 1.5B Instruct, en Ollama | LLM generativo | Conversar, recordar los últimos cuatro intercambios y decidir si llamar a imagen |
| SD-Turbo, en Diffusers | Difusión generativa texto → imagen | Crear una imagen nueva de 512 × 512 a partir del prompt visual |
| Voz instalada en el SO | TTS / síntesis de voz | Pronunciar el texto generado por el LLM |
| openFrameworks | Interfaz y obra | Mostrar respuesta/imagen y usar color y energía para modificar la experiencia |

Es **un coordinador con herramientas**. Qwen se consulta dos veces por turno:
primero redacta la conversación y luego propone la dirección visual. Son dos roles
sobre el mismo modelo, no varios agentes LLM autónomos. Tampoco es un único modelo multimodal: son modelos conectados.
No interpreta imágenes ni ve la webcam. La imagen previa se conserva en pantalla;
«cambiala» genera una nueva a partir de su descripción en el historial, no edita
sus píxeles. El modelo pequeño puede equivocarse o pedir una imagen cuando no corresponde.

## Instalar

Desde la raíz del repositorio:

| Sistema | Conversación y voz | Agregar generación de imágenes |
| --- | --- | --- |
| macOS | `bash install_env_mac.command --voice` | `bash install_env_mac.command --voice --images` |
| Linux | `bash install_env_linux.sh --voice` | `bash install_env_linux.sh --voice --images` |
| Windows CMD | `install_env_windows.cmd -Voice` | `install_env_windows.cmd -Voice -Images` |
| Windows PowerShell | `.\install_env_windows.cmd -Voice` | `.\install_env_windows.cmd -Voice -Images` |

El instalador prepara Python y Vosk, instala Ollama si falta y descarga Qwen.
`--images` agrega PyTorch/Diffusers y los pesos fp16 de SD-Turbo. No se descargan
modelos durante una conversación. Si Ollama ya está instalado, se reutiliza.
El instalador oficial de Ollama puede pedir permisos del sistema.

Qwen ocupa aproximadamente **1 GB**, Vosk **39 MB** de descarga y SD-Turbo alrededor
de **2.6 GB** de pesos. Las bibliotecas y cachés ocupan espacio adicional: prever
**8 GB libres para voz y 12 GB con imágenes**. Como orientación, 8 GB de RAM para
conversar y 16 GB para imágenes; no son mínimos medidos en todos los equipos.

Ollama elige GPU/CPU según la máquina. Diffusers elige CUDA → MPS → CPU según
lo que permita el PyTorch instalado; el indicador de dispositivo de PyTorch es
la comprobación real, no la presencia física de una GPU. Para CUDA en Linux puede
ser necesario instalar la rueda correspondiente desde [PyTorch](https://pytorch.org/get-started/locally/).
Mac Intel usa CPU. Con poca memoria, apagar IMAGEN. **Generar imágenes en CPU puede
tardar minutos**: esto no tiene la latencia de pose/tracking. Ollama libera el LLM
al terminar cada respuesta y la herramienta libera SD-Turbo tras cada imagen para
reducir la memoria simultánea (a costa de recargas).

## Actualizar y ejecutar OF

Copiar `voiceScene/src/` completo (incluye `ImageResponse.h`) sobre la copia del
SDK: `OF/apps/myApps/voiceScene/src/`. Mantener `bin/data/local-service.json`
apuntando a este repo. Se usa el mismo **ofxLocalAI**; no se agregó otro addon.
Si es una instalación nueva, seguir el Project Generator del [README general](../README.md).

Cerrar el ejemplo y el servidor anterior para que Python cargue el código nuevo.
En la carpeta de voiceScene del SDK, compilar y ejecutar:

```sh
make Release -j4
make RunRelease
```

Windows: compilar el proyecto generado de Visual Studio.

1. **CONECTAR** inicia/reutiliza Python. Ollama se inicia cuando llega el primer turno.
2. Escribir `Hola, soy Ana. Me gustaría imaginar una ciudad musical.` y ENVIAR.
3. Continuar `¿Cómo me llamo? ¿Qué materiales tendría esa ciudad?` para mostrar memoria.
4. Con IMAGEN encendido: `Dibujá esa ciudad al atardecer.` Esperar a Stable Diffusion.
5. **ESCUCHAR** prepara el micrófono: hablar cuando aparezca **LISTO**. Una pausa
   de 1.4 segundos termina el turno; espera inicial de 8 s y límite de 25 s por turno.
6. **AUTO** alterna escuchar → responder → hablar → escuchar. Se puede apagar
   durante un turno: evita el siguiente, pero no cancela el que ya está trabajando.
7. **VOZ** activa/desactiva el TTS del siguiente turno. **IMAGEN** habilita la
   herramienta del siguiente turno. **NUEVA CHARLA** borra el contexto de la interfaz
   creando otra sesión (solo cuando el turno terminó).

Esc sale del editor; C conecta, M escucha, V alterna voz e I imagen. En el editor
las letras se escriben. El texto aparece progresivamente por una animación de OF;
no es streaming de tokens. Los estados de trabajo sí se consultan mientras Python procesa.
El micrófono no se abre hasta ESCUCHAR/AUTO. Autorizar a la app/terminal en el SO.
Vosk pequeño puede transcribir mal nombres o frases; para comparar, repetir por texto.

## Elegir la voz de respuesta

Después de **CONECTAR**, abrir **Elegir voz: Automática (N) ▼** en la parte superior. El número es
la cantidad de voces recibidas del servidor. Muestra nombre e idioma; primero aparecen las voces españolas. **Anterior / Siguiente**
recorre la lista. **Automática** elige una voz española disponible, con preferencia
por Mónica o Paulina en macOS.

- Seleccionar una voz y pulsar **PROBAR VOZ**. Lee una frase fija, sin LLM,
  generación de imagen ni micrófono. La prueba suena aunque VOZ esté apagado.
- La elección se aplica a los próximos turnos y se guarda en
  `bin/data/voice-settings.json` del ejemplo de OF. Ese archivo no se versiona.
- Para cambiar de voz, esperar a que termine el turno actual. Probar una voz apaga
  AUTO para que el micrófono no se reactive al terminar la prueba.
- Si instalaste voces nuevas, pulsar **ACTUALIZAR** dentro del selector.
- Una voz guardada que ya no está disponible vuelve a **Automática**, con aviso.

**VOZ: ON/OFF** solo activa o desactiva la lectura; la elección del timbre está
en el desplegable superior. Si solo aparece Automática, esperar a que cargue el
catálogo o pulsar ACTUALIZAR: ese renglón no representa toda la lista.

Se usan voces del sistema: `say` en macOS, las voces habilitadas que expone
`System.Speech` en Windows y `espeak-ng`/`espeak` en Linux. El catálogo puede variar
entre equipos; algunas voces de otras aplicaciones de Windows no están disponibles
para System.Speech. Instalar voces desde la configuración correspondiente y actualizar.
Esto cambia el timbre y la pronunciación; no traduce el texto ni clona voces.

En `ofApp.cpp`, `selectedVoice` viaja como `voice` en el pedido JSON. `selectVoice()`
guarda la elección, `/voices` consulta el catálogo y `/voice/preview` inicia una
prueba. `realtime/tts.py` adapta los tres sistemas operativos; el LLM no elige la voz.
Si al conectar aparece «Servidor anterior», cerrar el Python que quedó abierto y
volver a conectar: esta versión agrega el catálogo de voces.

## Código a mostrar y modificar

En `src/ofApp.cpp` hay cuatro bloques numerados:

1. **`send()`**: prompt libre y flags → `POST /conversation/start`. La sesión
   permite conservar contexto. `microphone=true` pide a Python capturar audio.
2. **`update()`**: `POST /conversation/poll` recibe progreso sin bloquear `draw()`.
3. **`onResult()`**: `reply`, `color` y `energy` son variables de la obra.
   **V1:** comentar asignación base de energía y descomentar `1.f - ...` para invertirla.
4. **Carga de `image_base64` y `draw()`**: convertir JPEG/base64 en `ofImage` y dibujar.
   **V2:** comentar escala fija y descomentar la escala sinusoidal para hacer respirar
   la imagen con `energy`. La animación la hace OF; sus píxeles los generó SD-Turbo.

Para cambiar la personalidad o qué situaciones llaman a imagen, editar `SYSTEM`
en `realtime/llm.py`. Para ver la coordinación, `realtime/conversation.py` conecta
ASR → LLM → imagen opcional → TTS. No hay `eval`, ejecución de código del modelo
ni acceso a herramientas arbitrarias: se valida un contrato pequeño.

## Fallos visibles

- Falta Ollama/Qwen: repetir `--voice`; revisar `.local/ollama-voice.log`.
- Falta SD-Turbo o no alcanza la memoria: se conserva la respuesta conversacional;
  aparece el fallo de imagen. No se suplanta con un dibujo procedural.
- No reconoce voz: se detiene AUTO; probar otro micrófono o texto escrito.
- Fallo del LLM/red: se muestra el error y se detiene AUTO. Reconectar si hace falta.
- El puerto 8770 sigue en una versión anterior: cerrar el servicio que quedó abierto.

## Fuentes

[Qwen en Ollama](https://ollama.com/library/qwen2.5:1.5b-instruct),
[API de chat](https://docs.ollama.com/api/chat),
[Vosk](https://alphacephei.com/vosk/models),
[SD-Turbo: uso, límites y licencia](https://huggingface.co/stabilityai/sd-turbo).
SD-Turbo tiene condiciones propias; consultar su licencia para uso comercial.
