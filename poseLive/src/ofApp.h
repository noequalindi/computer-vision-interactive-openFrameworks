#pragma once
#include "ofMain.h"
#include "ofxLiveVision.h"
#include "ofxVisionDraw.h"
#include "ofxTone.h"
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
    // Mundo físico local: objetos y superficies; no ejecuta otro modelo ML.
    ofxBodyPhysics physics;
    const ofRectangle stage{24, 120, 784, 588};
    bool showPhysics = true;
    bool showCamera = true;
    bool showSkeleton = true;
    // Captura y transporte compartidos. No contiene la metáfora de esta obra.
    ofxLiveVision vision;
    // Sintetizador sinusoidal en OF; no es el TTS conversacional de voiceScene.
    ofxTone tone;
    // Última pose válida. Una lista vacía permite manejar ausencia de persona.
    ofJson landmarks = ofJson::array();
    // Velocidad normalizada de la muñeca: 0 quieta / 1 alcanza el límite elegido.
    float motion = 0;
    // Muestra anterior para comparar posiciones y estimar velocidad.
    glm::vec2 lastWrist{0, 0};
    // Tiempo de la muestra previa; -1 indica que todavía no hay comparación.
    float lastSample = -1;
};
