#pragma once
#include "ofMain.h"
#include "ofxLiveVision.h"
#include "ofxVisionDraw.h"
#include "ofxBodyPhysics.h"

// ESTADO DE LA OBRA: estas variables sobreviven entre cuadros.
// .h declara qué guarda/hace la app; .cpp implementa cómo lo hace.
class ofApp : public ofBaseApp
{
public:
    // Funciones que OF llama y callbacks a los que se suscribe el ejemplo.
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;
    void mousePressed(int x, int y, int button) override;
    void onResult(ofJson& result);

private:
    // Estado interno: cambiarlo en update/onResult y representarlo en draw.
    void drawInstructions();
    void updateDemo();
    // Captura y transporte compartidos. No contiene la metáfora de esta obra.
    ofxLiveVision vision;
    // Mundo físico local: objetos y superficies; no ejecuta otro modelo ML.
    ofxBodyPhysics physics;
    const ofRectangle stage{24, 120, 784, 588};
    // Bordes en píxeles del escenario, listos para dibujar o usar como colisión.
    std::vector<ofPolyline> contours;
    bool showCamera = true;
    bool showFill = true;
    bool rain = true;
    bool demo = false;
};
