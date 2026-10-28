#include "ofMain.h"
#include "ofApp.h"

// PUNTO DE ENTRADA: abre la ventana y entrega el ciclo de vida a OF.
// La conexión y las decisiones de la obra están en ofApp.cpp.
int main()
{
    ofGLFWWindowSettings settings;
    // Tamaño de interfaz; no cambia la resolución de inferencia del modelo.
    settings.setSize(1200, 800);
    settings.resizable = false;
    ofCreateWindow(settings);
    // OF llamará setup(), luego update()/draw(), y finalmente exit().
    ofRunApp(std::make_shared<ofApp>());
}
