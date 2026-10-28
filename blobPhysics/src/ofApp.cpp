// GUÍA DE CLASE: Selfie Segmenter + OpenCV -> contornos -> superficies de Box2D.
// Prácticas y recorrido del código: docs/GUIA_CODIGO_OF.md en el repositorio.

#include "ofApp.h"

// PREPARAR: OF llama una sola vez. Configuramos recursos y escuchamos respuestas.
// Conectar se hace por acción del usuario; registrar un listener no ejecuta el modelo.
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(12, 21, 38);
    vision.setup();
    vision.targetFps = 15; // Inferencia limitada; física y dibujo siguen a 60 Hz.
    ofAddListener(vision.response, this, &ofApp::onResult);
    physics.setup(stage);
}

// ACTUALIZAR: OF repite esta función mientras la ventana está abierta.
// Recibimos datos y avanzamos la obra; no esperamos una inferencia de forma bloqueante.
void ofApp::update()
{
    vision.update();
    if (demo)
    {
        updateDemo();
    }
    else
    {
        // [1] Webcam -> JPEG/base64 -> HTTP /vision. No bloquea el dibujo.
        if (vision.readyForFrame())
        {
            // CLASE BLOB-ENVIO: Python devuelve bordes simplificados de la máscara de persona.
            vision.sendFrame("silhouette");
        }
        // No dejamos una pared invisible si se pierde la cámara o la persona.
        if (!vision.fresh())
        {
            contours.clear();
            physics.clearBody();
        }
    }

    // [4] La IA ya terminó. Box2D resuelve gravedad y choques entre respuestas.
    physics.update(ofGetLastFrameTime(), rain);
}

// RECIBIR Y TRANSFORMAR: cada blob es una región conectada de la máscara.
// Convertimos su borde de coordenadas normalizadas a píxeles de nuestra escena.
void ofApp::onResult(ofJson& result)
{
    if (demo)
    {
        return;
    }
    contours.clear();
    // [2] La respuesta trae polígonos normalizados, no una imagen generativa.
    for (const auto& blob : result.value("blobs", ofJson::array()))
    {
        ofPolyline outline;
        for (const auto& point : blob["contour"])
        {
            outline.addVertex(stage.x + point["x"].get<float>() * stage.width,
                              stage.y + point["y"].get<float>() * stage.height);
        }
        if (outline.size() >= 3)
        {
            outline.close();
            contours.push_back(outline);
        }
    }
    // [3] Este es el vínculo entre detección e interacción: contorno -> collider.
    // CLASE BLOB-ACCION: quitar esta llamada conserva dibujo pero elimina colisiones.
    // VARIANTE BLOB1: comentar esta línea deja la figura visible sin colisiones.
    physics.setContours(contours);
}

// DIBUJAR: representar el estado actual. Cambiar colores, tamaños o composición
// aquí modifica la obra sin cambiar los pesos del modelo.
void ofApp::draw()
{
    ofSetColor(235);
    ofDrawBitmapString("UNA / INFORMATICA APLICADA 2   -   BLOB PHYSICS", 24, 45);
    ofSetColor(165, 190, 220);
    ofDrawBitmapString("PERSONA -> MASCARA -> CONTORNO -> COLISIONES", 24, 87);
    ofSetColor(20, 32, 51);
    ofDrawRectangle(stage);
    if (showCamera && !demo)
    {
        vision.drawCamera(stage);
    }

    // [5] La misma geometría puede ser una pared, una máscara o un pincel.
    for (const auto& contour : contours)
    {
        if (showFill)
        {
            ofPath shape;
            shape.setFillColor(ofColor(55, 100, 175, 180));
            for (std::size_t i = 0; i < contour.size(); ++i)
            {
                if (i == 0)
                {
                    shape.moveTo(contour[i]);
                }
                else
                {
                    shape.lineTo(contour[i]);
                }
            }
            shape.close();
            shape.draw();
        }
        ofSetColor(155, 190, 255);
        ofSetLineWidth(3);
        contour.draw();
    }
    ofSetLineWidth(1);
    physics.draw();

    ofSetColor(240);
    if (demo)
    {
        ofDrawBitmapStringHighlight("DEMO SIN MODELO / mover el mouse / C: usar webcam", 40, 148);
    }
    else if (contours.empty())
    {
        ofDrawBitmapStringHighlight("C: conectar / mostrar torso y brazos con buena luz", 40, 148);
    }
    drawInstructions();
    drawConnect(vision.service.status);
}

// AYUDA: estos textos y botones describen controles; no intervienen en la inferencia.
void ofApp::drawInstructions()
{
    ofSetColor(24, 39, 63);
    ofDrawRectRounded(832, 110, 348, 600, 10);
    ofSetColor(235);
    ofDrawBitmapString("TU CUERPO ES UNA SUPERFICIE", 850, 142);
    ofSetColor(180, 205, 235);
    ofDrawBitmapString("1 / C: conectar camara y Python.\n\n"
                       "2 / Mostrar cabeza y torso.\n"
                       "    Extender brazos lentamente.\n\n"
                       "3 / Sostener las esferas sobre\n"
                       "    la cabeza o los brazos.\n\n"
                       "El fondo no necesita ser verde.",
                       850, 179);

    const std::string labels[] = {"V: VIDEO " + std::string(showCamera ? "ON" : "OFF"),
                                  "M: RELLENO " + std::string(showFill ? "ON" : "OFF"),
                                  "L: LLUVIA " + std::string(rain ? "ON" : "OFF"),
                                  "R: LIMPIAR ESFERAS",
                                  "D: DEMO SIN CAMARA " + std::string(demo ? "ON" : "OFF")};
    for (int i = 0; i < 5; ++i)
    {
        ofSetColor(45, 69, 111);
        ofDrawRectRounded(850, 350 + 45 * i, 310, 36, 7);
        ofSetColor(255);
        ofDrawBitmapString(labels[i], 862, 373 + 45 * i);
    }
    ofSetColor(180, 205, 235);
    ofDrawBitmapString("ESPACIO: agregar esfera\nContornos: " + ofToString(contours.size()) +
                           " / esferas: " + ofToString(physics.count()),
                       850, 600);
    ofDrawBitmapString("OF: " + ofToString(ofGetFrameRate(), 0) + " FPS / modelo: " +
                           (demo ? std::string("DEMO") : ofToString(vision.inferenceMs, 1) + " ms"),
                       850, 645);
    ofSetColor(145, 170, 200);
    ofDrawBitmapString("Selfie Segmenter + OpenCV + Box2D\nSilueta 2D; no mide profundidad.", 850,
                       681);
}

// DEMO DIDÁCTICA: fabrica una figura para probar la física con el mouse.
// No llama a un modelo. Mantener visible esta distinción al mostrarla en clase.
void ofApp::updateDemo()
{
    // Sólo para explicar física sin permisos de cámara; NO es detección ni ML.
    // C vuelve al modelo real. Figura simplificada que sigue al mouse.
    const float center = ofClamp(float(ofGetMouseX()), stage.x + 180, stage.getRight() - 180);
    const glm::vec2 vertices[] = {{-38, -175},  {0, -190},   {38, -175}, {45, -115},  {28, -95},
                                  {75, -80},    {170, -120}, {180, -95}, {100, -25},  {65, -30},
                                  {70, 180},    {-70, 180},  {-65, -30}, {-100, -25}, {-180, -95},
                                  {-170, -120}, {-75, -80},  {-28, -95}, {-45, -115}};
    ofPolyline outline;
    for (auto point : vertices)
    {
        outline.addVertex(center + point.x, stage.y + 350 + point.y);
    }
    outline.close();
    contours = {outline};
    physics.setContours(contours);
}

// CONTROLES: traducir teclas a acciones. Los estados quedan guardados en ofApp.h.
void ofApp::keyPressed(int key)
{
    if (key == 'c' || key == 'C')
    {
        demo = false;
        contours.clear();
        physics.clearBody();
        vision.connect();
    }
    if (key == 'v' || key == 'V')
    {
        showCamera = !showCamera;
    }
    if (key == 'm' || key == 'M')
    {
        showFill = !showFill;
    }
    if (key == 'l' || key == 'L')
    {
        rain = !rain;
    }
    if (key == 'r' || key == 'R')
    {
        physics.reset();
    }
    if (key == ' ')
    {
        physics.spawn();
    }
    if (key == 'd' || key == 'D')
    {
        demo = !demo;
        contours.clear();
        physics.clearBody();
        physics.reset();
    }
}

// BOTONES: comprobar si el clic cae dentro de un rectángulo y activar su acción.
void ofApp::mousePressed(int x, int y, int button)
{
    if (connectClicked(x, y))
    {
        keyPressed('c');
    }
    const char actions[] = {'v', 'm', 'l', 'r', 'd'};
    for (int i = 0; i < 5; ++i)
    {
        if (ofRectangle(850, 350 + 45 * i, 310, 36).inside(x, y))
        {
            keyPressed(actions[i]);
        }
    }
}

// CERRAR: quitar listeners antes de destruir el receptor y liberar cámara/servicio.
// El addon sólo detiene el Python que inició; conserva un servidor iniciado aparte.
void ofApp::exit()
{
    ofRemoveListener(vision.response, this, &ofApp::onResult);
    vision.close();
}
