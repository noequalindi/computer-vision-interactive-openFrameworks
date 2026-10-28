// GUÍA DE CLASE: EfficientDet -> objetos con cajas -> metáfora visual elegida en OF.
// Prácticas y recorrido del código: docs/GUIA_CODIGO_OF.md en el repositorio.

#include "ofApp.h"

// PREPARAR: OF llama una sola vez. Configuramos recursos y escuchamos respuestas.
// Conectar se hace por acción del usuario; registrar un listener no ejecuta el modelo.
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(12, 21, 38);
    vision.setup();
    vision.targetFps = 10; // Reducir esto baja CPU; OF sigue dibujando a 60.
    ofAddListener(vision.response, this, &ofApp::onResult);
}

// ACTUALIZAR: OF repite esta función mientras la ventana está abierta.
// Recibimos datos y avanzamos la obra; no esperamos una inferencia de forma bloqueante.
void ofApp::update()
{
    vision.update();
    // [1] La misma conexión, otro modelo y otro contrato de salida.
    // CLASE OBJETOS-ENVIO: limitar el envío a un pedido en vuelo.
    // La tarea devuelve varias cajas; no una clasificación única de toda la foto.
    if (vision.readyForFrame())
    {
        vision.sendFrame("objects");
    }
    // Sin datos recientes, retirar cajas para no dibujar detecciones antiguas.
    if (!vision.fresh())
    {
        objects = ofJson::array();
    }
}

// RECIBIR: conservar la última lista para usarla en draw(). Una lista vacía
// es válida: significa que en ese cuadro no hubo detecciones aceptadas.
void ofApp::onResult(ofJson& result)
{
    // [2] Lista de objetos: categoría, score, caja normalizada e ID temporal.
    objects = result["objects"];
}

// DIBUJAR: representar el estado actual. Cambiar colores, tamaños o composición
// aquí modifica la obra sin cambiar los pesos del modelo.
void ofApp::draw()
{
    ofSetColor(230);
    ofDrawBitmapString("UNA / INFORMATICA APLICADA 2   -   OBJECT LIVE", 24, 45);
    const ofRectangle area{24, 110, 800, 600};
    vision.drawCamera(area);
    int row = 0;
    // VARIANTE O1 / Elegir UNA línea: qué objeto activa la metáfora.
    const std::string activeLabel = "cup"; // BASE: taza.
    // const std::string activeLabel = "bottle"; // ALTERNATIVA: botella.
    for (const auto& item : objects)
    {
        // [3] Coordenadas 0..1 -> píxeles del espacio de proyección.
        ofRectangle box{area.x + item["x"].get<float>() * area.width,
                        area.y + item["y"].get<float>() * area.height,
                        item["w"].get<float>() * area.width, item["h"].get<float>() * area.height};
        ofNoFill();
        ofSetColor(100, 200, 255);
        ofDrawRectangle(box);
        ofFill();
        const std::string label = item["label"].get<std::string>();
        ofDrawBitmapString(label + " #" + ofToString(item["id"].get<int>()), box.x, box.y + 15);
        ofDrawBitmapString(label + " " + ofToString(item["score"].get<float>(), 2), 850,
                           280 + row++ * 25);
        // [4] Una taza cotidiana se vuelve un pequeño sol: metáfora visual.
        // Cambiar "cup" por otra etiqueta COCO permite modificar la interacción.
        // CLASE OBJETOS-ACCION: este if es una decisión de la instalación.
        // El modelo reconoce cup; nosotros decidimos convertirla en un sol.
        if (label == activeLabel)
        {
            const auto center = box.getCenter();
            // VARIANTE O2 / Elegir UNA línea: anillos o disco sólido.
            ofNoFill(); // BASE: el objeto es un sol de anillos.
            // ofFill(); // ALTERNATIVA: el objeto se vuelve un disco.
            for (int ring = 0; ring < 3; ++ring)
            {
                ofDrawCircle(center,
                             box.width * 0.5f + ring * 15 + 5 * std::sin(ofGetElapsedTimef() * 2));
            }
            ofFill();
        }
    }
    ofSetColor(180, 205, 240);
    ofDrawBitmapString(
        "EfficientDet-Lite0 int8\n\nMostrar taza / botella\n\nEtiqueta + caja + score\nIDs por cercania (no ML).\nPueden cambiar al cruzarse.",
        850, 140);
    ofDrawBitmapString("Objetos: " + ofToString(objects.size()) +
                           "\nModelo: " + ofToString(vision.inferenceMs, 1) +
                           " ms\nIda/vuelta: " + ofToString(vision.roundTripMs, 1) + " ms",
                       850, 560);
    drawConnect(vision.service.status);
}

// CONTROLES: traducir teclas a acciones. Los estados quedan guardados en ofApp.h.
void ofApp::keyPressed(int key)
{
    if (key == 'c' || key == 'C')
    {
        vision.connect();
    }
}

// BOTONES: comprobar si el clic cae dentro de un rectángulo y activar su acción.
void ofApp::mousePressed(int x, int y, int button)
{
    if (connectClicked(x, y))
    {
        vision.connect();
    }
}

// CERRAR: quitar listeners antes de destruir el receptor y liberar cámara/servicio.
// El addon sólo detiene el Python que inició; conserva un servidor iniciado aparte.
void ofApp::exit()
{
    ofRemoveListener(vision.response, this, &ofApp::onResult);
    vision.close();
}
