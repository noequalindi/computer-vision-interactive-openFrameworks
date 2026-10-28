// GUÍA DE CLASE: Pose Lite -> apertura de brazos -> tipografía, texturas y sonido.
// Prácticas y recorrido del código: docs/GUIA_CODIGO_OF.md en el repositorio.

#include "ofApp.h"

namespace
{
const ofRectangle poemArea{24, 116, 1152, 420};
const char* textureNames[] = {"AUTO / GESTO", "GRANO", "RAYAS", "PUNTOS"};
const char* paletteNames[] = {"OCEANO / CORAL", "LILA / LIMA", "FUEGO / ROSA"};
const ofColor coldColors[] = {{75, 185, 255}, {154, 113, 255}, {255, 179, 60}};
const ofColor warmColors[] = {{255, 111, 149}, {211, 250, 116}, {255, 83, 132}};
} // namespace

// PREPARAR: OF llama una sola vez. Configuramos recursos y escuchamos respuestas.
// Conectar se hace por acción del usuario; registrar un listener no ejecuta el modelo.
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(10, 17, 30);
    ofEnableAlphaBlending();
    vision.setup();
    ofAddListener(vision.response, this, &ofApp::onResult);

    // Fuente incluida en bin/data: misma apariencia al copiar el ejemplo a otro equipo.
    fontReady = poemFont.load("fonts/Poppins-ExtraBold.ttf", 112, true, true, false, 0.3f, 72);
    if (!fontReady)
    {
        fontReady = poemFont.load(OF_TTF_SANS, 112, true, true, false, 0.3f, 72);
    }

    // FBO: una imagen en memoria GPU. Blanco = letra; alfa 0 = fondo transparente.
    letterMask.allocate(int(poemArea.width), int(poemArea.height), GL_RGBA);
    textureReady = material.load("shaders/poem");
}

// ACTUALIZAR: OF repite esta función mientras la ventana está abierta.
// Recibimos datos y avanzamos la obra; no esperamos una inferencia de forma bloqueante.
void ofApp::update()
{
    vision.update();

    if (demoMode)
    {
        // Modo didáctico explícito: reemplazamos la medida por mouse, no por ML.
        targetOpenness = ofMap(ofGetMouseX(), poemArea.x, poemArea.getRight(), 0, 1, true);
    }
    else
    {
        if (vision.readyForFrame())
        {
            // CLASE POEMA-ENVIO: mismo modelo de poseLive, otra interpretación artística.
            vision.sendFrame("pose");
        }
        if (!vision.fresh())
        {
            targetOpenness = 0;
            landmarks = ofJson::array();
        }
    }

    // Interpolar entre respuestas mantiene fluidez aunque la inferencia vaya más lenta.
    const float blend = 1 - std::exp(-6 * ofGetLastFrameTime());
    openness += (targetOpenness - openness) * blend;
    tone.frequency = 130 + openness * 440;
    const bool validGesture = visiblePoint(landmarks, 15) && visiblePoint(landmarks, 16);
    tone.volume = (demoMode || validGesture) ? openness * 0.12f : 0;
}

// RECIBIR Y MEDIR: usar hombros y muñecas visibles para construir una apertura.
// El modelo devuelve puntos; la relación entre brazos y poema la elegimos nosotros.
void ofApp::onResult(ofJson& result)
{
    if (demoMode)
    {
        return; // Ignorar una respuesta que estaba en vuelo al entrar en demo.
    }

    landmarks = result["landmarks"];
    targetOpenness = 0;
    if (!visiblePoint(landmarks, 15) || !visiblePoint(landmarks, 16) ||
        !visiblePoint(landmarks, 11) || !visiblePoint(landmarks, 12))
    {
        return;
    }

    const ofRectangle normalized{0, 0, 1, 1};
    const float wrists = glm::distance(visionPoint(landmarks[15], normalized),
                                       visionPoint(landmarks[16], normalized));
    const float shoulders = glm::distance(visionPoint(landmarks[11], normalized),
                                          visionPoint(landmarks[12], normalized));

    // [1] Regla geométrica. No clasifica emociones: nosotros diseñamos la metáfora.
    // CLASE POEMA-ACCION: dividir por ancho de hombros da una referencia corporal.
    // Es una heurística en imagen, no una medida 3D; max evita dividir por casi cero.
    // ofMap toma el intervalo 1..3 y lo lleva a 0..1, recortando extremos con true.
    targetOpenness = ofMap(wrists / std::max(0.05f, shoulders), 1, 3, 0, 1, true);
}

// TIPOGRAFÍA: dibujar letras blancas sobre transparencia dentro de un FBO.
// Esta máscara sirve de recorte para los materiales; no es segmentación del cuerpo.
void ofApp::drawPoemMask()
{
    // VARIANTE B2 / Elegir UNA línea. \n separa el poema en dos renglones.
    const std::string poem = "EL ESPACIO\nRESPIRA"; // BASE.
    // const std::string poem = "ME ABRO\nY ME CIERRO"; // ALTERNATIVA.

    // VARIANTE B1 / Elegir UNA línea: expansión o contradicción del gesto.
    const float expansion = openness; // BASE: abrir brazos expande el poema.
    // const float expansion = 1.f - openness; // ALTERNATIVA: abrir brazos comprime el poema.
    const float tracking = ofLerp(1.0f, 1.28f, expansion);
    poemFont.setLetterSpacing(tracking);

    const auto lines = ofSplitString(poem, "\n", true);
    if (lines.empty())
    {
        return;
    }

    float widest = 1;
    float tallest = 1;
    for (const auto& line : lines)
    {
        const auto bounds = poemFont.getStringBoundingBox(line, 0, 0);
        widest = std::max(widest, bounds.width);
        tallest = std::max(tallest, bounds.height);
    }

    // [2] Letra grande, pero siempre dentro del cuadro, incluso con la variante B2.
    const float desiredScale = ofLerp(0.92f, 1.22f, expansion);
    const float fitWidth = (poemArea.width - 96) / widest;
    const float rowHeight = poemArea.height / lines.size();
    const float fitHeight = (rowHeight - 40) / tallest;
    const float scale = std::min(desiredScale, std::min(fitWidth, fitHeight));

    letterMask.begin();
    ofClear(0, 0, 0, 0);
    ofSetColor(255);
    for (size_t row = 0; row < lines.size(); ++row)
    {
        const auto bounds = poemFont.getStringBoundingBox(lines[row], 0, 0);
        ofPushMatrix();
        ofTranslate(poemArea.width / 2,
                    rowHeight * (row + 0.5f) +
                        std::sin(ofGetElapsedTimef() * 0.8f + row) * openness * 7);
        ofScale(scale, scale);
        poemFont.drawString(lines[row], -bounds.x - bounds.width / 2,
                            -bounds.y - bounds.height / 2);
        ofPopMatrix();
    }
    letterMask.end();
}

// MATERIAL: pintar la máscara con un shader y parámetros del gesto.
// Si el shader no carga, conservar una versión legible con color plano.
void ofApp::drawPoem()
{
    ofSetColor(15, 26, 44);
    ofDrawRectRounded(poemArea, 14);
    if (!fontReady || !letterMask.isAllocated())
    {
        ofSetColor(240);
        ofDrawBitmapString("No se pudo preparar la tipografia. Revisar bin/data/fonts.", 100, 300);
        return;
    }

    drawPoemMask();
    ofSetColor(255);
    if (textureReady)
    {
        // [3] Una única medida controla el color y la mezcla de texturas.
        // El shader genera patrones baratos en GPU; no son imágenes de otro modelo.
        material.begin();
        material.setUniformTexture("letterMask", letterMask.getTexture(), 0);
        material.setUniform2f("resolution", poemArea.width, poemArea.height);
        material.setUniform1f("time", ofGetElapsedTimef());
        material.setUniform1f("openness", openness);
        material.setUniform1i("patternMode", textureMode);
        const ofFloatColor cold = coldColors[paletteIndex];
        const ofFloatColor warm = warmColors[paletteIndex];
        material.setUniform3f("coldColor", cold.r, cold.g, cold.b);
        material.setUniform3f("warmColor", warm.r, warm.g, warm.b);
        letterMask.draw(poemArea.x, poemArea.y);
        material.end();
    }
    else
    {
        // Si falta el shader, el poema sigue siendo legible con color plano.
        ofSetColor(coldColors[paletteIndex].getLerped(warmColors[paletteIndex], openness));
        letterMask.draw(poemArea.x, poemArea.y);
    }
}

// DIBUJAR: representar el estado actual. Cambiar colores, tamaños o composición
// aquí modifica la obra sin cambiar los pesos del modelo.
void ofApp::draw()
{
    ofSetColor(235);
    ofDrawBitmapString("UNA / INFORMATICA APLICADA 2   -   BODY POEM", 24, 44);
    ofSetColor(158, 183, 215);
    ofDrawBitmapString("ABRIR EL CUERPO / CAMBIAR LA MATERIA DE LAS PALABRAS", 24, 80);

    drawPoem();
    drawControls();
    drawConnect(demoMode ? "DEMO SIN MODELO / mover el mouse sobre el poema. D vuelve a pose."
                         : vision.service.status);
}

// INTERFAZ: miniatura de cámara, barra de apertura y botones.
// Mostrar la medida ayuda a entender qué entrada está controlando la obra.
void ofApp::drawControls()
{
    const ofRectangle preview{24, 558, 240, 180};
    if (demoMode)
    {
        ofSetColor(22, 37, 59);
        ofDrawRectRounded(preview, 8);
        ofSetColor(190, 211, 240);
        ofDrawBitmapString("DEMO SIN MODELO\n\nMouse izquierda: cerrar\nMouse derecha: abrir", 38,
                           600);
    }
    else
    {
        vision.drawCamera(preview);
        drawSkeleton(landmarks, preview);
    }

    ofSetColor(200, 219, 243);
    ofDrawBitmapString("APERTURA " + ofToString(openness * 100, 0) + "%  /  " +
                           (demoMode ? "MOUSE" : "POSE LITE"),
                       294, 578);
    ofSetColor(35, 52, 76);
    ofDrawRectangle(294, 594, 510, 8);
    ofSetColor(coldColors[paletteIndex].getLerped(warmColors[paletteIndex], openness));
    ofDrawRectangle(294, 594, 510 * openness, 8);
    ofSetColor(155, 181, 213);
    ofDrawBitmapString("Brazos: tamano + separacion + color + textura", 294, 626);

    ofSetColor(35, 66, 111);
    ofDrawRectRounded(294, 645, 250, 44, 8);
    ofDrawRectRounded(558, 645, 246, 44, 8);
    ofDrawRectRounded(834, 558, 342, 44, 8);
    ofDrawRectRounded(834, 614, 342, 44, 8);
    ofSetColor(245);
    ofDrawBitmapString("T: " + std::string(textureNames[textureMode]), 308, 672);
    ofDrawBitmapString("P: " + std::string(paletteNames[paletteIndex]), 572, 672);
    ofDrawBitmapString(demoMode ? "D: VOLVER AL CUERPO" : "D: PROBAR CON MOUSE", 850, 585);
    ofDrawBitmapString("S: SONIDO " + std::string(tone.enabled ? "ACTIVADO" : "APAGADO"), 850, 641);
    ofSetColor(155, 181, 213);
    ofDrawBitmapString("C: conectar camara / Python\nS: activar audio con volumen bajo", 834, 690);
    ofDrawBitmapString(textureReady ? "AUTO: grano -> rayas -> puntos al abrir brazos."
                                    : "Sin textura: revisar bin/data/shaders y consola.",
                       294, 720);
}

// CONTROLES: traducir teclas a acciones. Los estados quedan guardados en ofApp.h.
void ofApp::keyPressed(int key)
{
    if (key == 'c' || key == 'C')
    {
        demoMode = false;
        vision.connect();
    }
    if (key == 's' || key == 'S')
    {
        tone.toggle();
    }
    if (key == 't' || key == 'T')
    {
        textureMode = (textureMode + 1) % 4;
    }
    if (key == 'p' || key == 'P')
    {
        paletteIndex = (paletteIndex + 1) % 3;
    }
    if (key == 'd' || key == 'D')
    {
        demoMode = !demoMode;
        landmarks = ofJson::array();
        targetOpenness = 0;
    }
}

// BOTONES: comprobar si el clic cae dentro de un rectángulo y activar su acción.
void ofApp::mousePressed(int x, int y, int button)
{
    if (connectClicked(x, y))
    {
        keyPressed('c');
    }
    if (ofRectangle(294, 645, 250, 44).inside(x, y))
    {
        keyPressed('t');
    }
    if (ofRectangle(558, 645, 246, 44).inside(x, y))
    {
        keyPressed('p');
    }
    if (ofRectangle(834, 558, 342, 44).inside(x, y))
    {
        keyPressed('d');
    }
    if (ofRectangle(834, 614, 342, 44).inside(x, y))
    {
        keyPressed('s');
    }
}

// CERRAR: quitar listeners antes de destruir el receptor y liberar cámara/servicio.
// El addon sólo detiene el Python que inició; conserva un servidor iniciado aparte.
void ofApp::exit()
{
    tone.close();
    ofRemoveListener(vision.response, this, &ofApp::onResult);
    vision.close();
}
