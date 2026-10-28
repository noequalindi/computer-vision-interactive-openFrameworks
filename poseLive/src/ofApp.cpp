// GUÍA DE CLASE: Pose Lite -> puntos -> movimiento / colisiones / sonido.
// Prácticas y recorrido del código: docs/GUIA_CODIGO_OF.md en el repositorio.

#include "ofApp.h"

namespace
{
// VARIANTE P1 / Elegir UNA línea: ¿qué parte del cuerpo controla la obra?
constexpr int trackedPoint = 16; // BASE: muñeca derecha anatómica.
// constexpr int trackedPoint = 15; // ALTERNATIVA: muñeca izquierda.
} // namespace

// PREPARAR: OF llama una sola vez. Configuramos recursos y escuchamos respuestas.
// Conectar se hace por acción del usuario; registrar un listener no ejecuta el modelo.
void ofApp::setup()
{
    ofSetFrameRate(60); // Dibujo independiente de los FPS de inferencia.
    ofBackground(12, 21, 38);
    vision.setup();
    physics.setup(stage);
    ofAddListener(vision.response, this, &ofApp::onResult);
}

// ACTUALIZAR: OF repite esta función mientras la ventana está abierta.
// Recibimos datos y avanzamos la obra; no esperamos una inferencia de forma bloqueante.
void ofApp::update()
{
    // [1] Actualiza cámara, conexión y respuestas; nunca espera al modelo.
    vision.update();

    // [2] Acá se solicita la inferencia. El addon descarta cuadros intermedios.
    if (vision.readyForFrame())
    {
        // CLASE POSE-ENVIO: se envía una imagen, no un prompt escrito.
        // "pose" selecciona la tarea que Python resolverá con Pose Lite.
        vision.sendFrame("pose");
    }

    // Sin datos recientes no inventamos movimiento ni dejamos sonido sostenido.
    if (!vision.fresh())
    {
        landmarks = ofJson::array();
        physics.clearBody();
        motion = 0;
        lastSample = -1;
    }

    // [5] Box2D corre a 60 pasos/segundo, separado de los pedidos al modelo.
    if (showPhysics)
    {
        physics.update(ofGetLastFrameTime());
    }

    // VARIANTE P2 / Elegir UNA línea: invertir el sentido del sonido.
    tone.frequency = 150 + motion * 500; // BASE: más movimiento -> más agudo.
    // tone.frequency = 650 - motion * 500; // ALTERNATIVA: más movimiento -> más grave.
    tone.volume = motion * 0.1f; // Si no hay movimiento, no hay volumen.
}

// RECIBIR Y TRANSFORMAR: llega un JSON ya procesado, en el hilo principal.
// Guardamos puntos, actualizamos colisiones y calculamos una medida de movimiento.
void ofApp::onResult(ofJson& result)
{
    // [3] La respuesta trae 33 puntos, no una imagen dibujada por la IA.
    landmarks = result["landmarks"];
    // [4] Convertimos la pose en superficies: pueden sostener/empujar esferas.
    // VARIANTE P4: comentar esta línea deja objetos con gravedad pero sin cuerpo.
    // CLASE POSE-ACCION: los puntos se convierten en superficies físicas.
    physics.setPose(landmarks);
    const float now = ofGetElapsedTimef();

    if (visiblePoint(landmarks, trackedPoint))
    {
        glm::vec2 wrist{landmarks[trackedPoint]["x"].get<float>(),
                        landmarks[trackedPoint]["y"].get<float>()};

        if (lastSample > 0 && now - lastSample < 1)
        {
            // Distancia / tiempo estima velocidad en coordenadas de imagen.
            // El clamp la lleva a 0..1; no estamos midiendo metros por segundo.
            motion =
                ofClamp(glm::distance(wrist, lastWrist) / std::max(0.01f, now - lastSample), 0, 1);
        }

        lastWrist = wrist;
        lastSample = now;
    }
    else
    {
        motion = 0;
        lastSample = -1;
    }
}

// DIBUJAR: representar el estado actual. Cambiar colores, tamaños o composición
// aquí modifica la obra sin cambiar los pesos del modelo.
void ofApp::draw()
{
    ofSetColor(230);
    ofDrawBitmapString("UNA / INFORMATICA APLICADA 2   -   POSE LIVE", 24, 45);
    ofSetColor(165, 190, 220);
    ofDrawBitmapString("CAMARA -> POSE -> COLISIONES + GRAVEDAD / SONIDO", 24, 87);

    const auto& area = stage;
    ofSetColor(20, 32, 51);
    ofDrawRectangle(area);

    if (showCamera)
    {
        vision.drawCamera(area);
    }
    if (showSkeleton)
    {
        drawSkeleton(landmarks, area);
    }

    if (showPhysics)
    {
        physics.draw();
    }

    // VARIANTE P3 / Agregar quitando // en la siguiente línea; comentar para volver.
    // El círculo representa movimiento, no un objeto detectado por el modelo.
    ofSetColor(110, 200, 255, 100);
    // ofDrawCircle(area.getCenter(), 20 + motion * 100);

    if (!vision.camera.isInitialized())
    {
        ofSetColor(220);
        ofDrawBitmapString(
            "1. Pulsar CONECTAR (C)\n2. Permitir acceso a la camara\n3. Mostrar torso y manos\n\nEl modelo se carga con la primera imagen.",
            190, 345);
    }
    else if (landmarks.empty())
    {
        ofSetColor(255);
        ofDrawBitmapStringHighlight("Esperando pose: mostrar torso y manos con buena luz.", 40,
                                    690);
    }

    drawInstructions();
    drawConnect(vision.service.status);
}

// AYUDA: estos textos y botones describen controles; no intervienen en la inferencia.
void ofApp::drawInstructions()
{
    // Ayuda siempre visible: cada control indica su estado y qué cambia.
    ofSetColor(24, 39, 63);
    ofDrawRectRounded(832, 110, 348, 600, 10);
    ofSetColor(230);
    ofDrawBitmapString("COMO EMPEZAR", 850, 140);
    ofSetColor(175, 200, 230);
    ofDrawBitmapString("1 / C: conectar Python + camara\n"
                       "    Primera carga: esperar.\n\n"
                       "2 / Mostrar torso y manos.\n"
                       "    Mover la muneca indicada.\n\n"
                       "3 / S: activar audio y moverse.\n"
                       "    Sin movimiento no suena.",
                       850, 165);

    // Las teclas y los botones llaman a la misma acción en keyPressed().
    ofSetColor(showCamera ? ofColor(45, 85, 145) : ofColor(42, 49, 65));
    ofDrawRectRounded(850, 320, 150, 42, 7);
    ofSetColor(showSkeleton ? ofColor(45, 85, 145) : ofColor(42, 49, 65));
    ofDrawRectRounded(1010, 320, 150, 42, 7);
    ofSetColor(255);
    ofDrawBitmapString("V: VIDEO " + std::string(showCamera ? "ON" : "OFF"), 863, 346);
    ofDrawBitmapString("E: PUNTOS " + std::string(showSkeleton ? "ON" : "OFF"), 1020, 346);

    ofSetColor(tone.enabled ? ofColor(45, 85, 145) : ofColor(42, 49, 65));
    ofDrawRectRounded(850, 376, 310, 42, 7);
    ofSetColor(255);
    ofDrawBitmapString("S: SONIDO " + std::string(tone.enabled ? "ACTIVADO" : "APAGADO"), 866, 402);
    ofSetColor(showPhysics ? ofColor(45, 85, 145) : ofColor(42, 49, 65));
    ofDrawRectRounded(850, 432, 150, 42, 7);
    ofSetColor(42, 49, 65);
    ofDrawRectRounded(1010, 432, 150, 42, 7);
    ofSetColor(255);
    ofDrawBitmapString("P: FISICA " + std::string(showPhysics ? "ON" : "OFF"), 860, 458);
    ofDrawBitmapString("R: REINICIAR", 1020, 458);
    ofSetColor(175, 200, 230);
    ofDrawBitmapString(
        "Brazos = superficies. Sostener\nlas esferas o empujarlas.\nESPACIO: agregar una esfera.",
        850, 498);
    ofSetColor(40, 58, 82);
    ofDrawRectangle(850, 550, 310, 12);
    ofSetColor(80, 160, 240);
    ofDrawRectangle(850, 550, motion * 310, 12);
    ofSetColor(185, 210, 240);
    ofDrawBitmapString("Movimiento: " + ofToString(motion, 2) +
                           " / esferas: " + ofToString(physics.count()),
                       850, 590);
    ofDrawBitmapString("Dibujo: " + ofToString(ofGetFrameRate(), 0) +
                           " FPS\nModelo: " + ofToString(vision.inferenceMs, 1) +
                           " ms\nIda/vuelta: " + ofToString(vision.roundTripMs, 1) + " ms",
                       850, 620);
    ofSetColor(145, 170, 200);
    ofDrawBitmapString("Pose Lite estima. Box2D simula.\nS: audio / V: video / E: puntos", 850,
                       682);
}

// CONTROLES: traducir teclas a acciones. Los estados quedan guardados en ofApp.h.
void ofApp::keyPressed(int key)
{
    if (key == 'p' || key == 'P')
    {
        showPhysics = !showPhysics;
    }
    if (key == 'r' || key == 'R')
    {
        physics.reset();
    }
    if (key == ' ' && showPhysics)
    {
        physics.spawn();
    }
    if (key == 'c' || key == 'C')
    {
        vision.connect();
    }
    if (key == 's' || key == 'S')
    {
        tone.toggle();
    }
    if (key == 'v' || key == 'V')
    {
        showCamera = !showCamera;
    }
    if (key == 'e' || key == 'E')
    {
        showSkeleton = !showSkeleton;
    }
}

// BOTONES: comprobar si el clic cae dentro de un rectángulo y activar su acción.
void ofApp::mousePressed(int x, int y, int button)
{
    if (ofRectangle(850, 432, 150, 42).inside(x, y))
    {
        keyPressed('p');
    }
    if (ofRectangle(1010, 432, 150, 42).inside(x, y))
    {
        keyPressed('r');
    }
    if (connectClicked(x, y))
    {
        keyPressed('c');
    }
    if (ofRectangle(850, 320, 150, 42).inside(x, y))
    {
        keyPressed('v');
    }
    if (ofRectangle(1010, 320, 150, 42).inside(x, y))
    {
        keyPressed('e');
    }
    if (ofRectangle(850, 376, 310, 42).inside(x, y))
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
