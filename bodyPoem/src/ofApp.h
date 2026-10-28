#pragma once
#include "ofMain.h"
#include "ofxLiveVision.h"
#include "ofxVisionDraw.h"
#include "ofxTone.h"

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
    void drawPoemMask();
    void drawPoem();
    void drawControls();

    // Captura y transporte compartidos. No contiene la metáfora de esta obra.
    ofxLiveVision vision;
    // Sintetizador sinusoidal en OF; no es el TTS conversacional de voiceScene.
    ofxTone tone;
    // Última pose válida. Una lista vacía permite manejar ausencia de persona.
    ofJson landmarks = ofJson::array();
    // Valor suavizado que mueve dibujo y sonido entre respuestas.
    float openness = 0;
    // Última medida del cuerpo; no es emoción ni intención inferida.
    float targetOpenness = 0;

    // La tipografía define la silueta; el shader pinta solo dentro de las letras.
    ofTrueTypeFont poemFont;
    ofFbo letterMask;
    ofShader material;
    bool textureReady = false;
    bool fontReady = false;
    bool demoMode = false; // D: explorar el dibujo sin cámara ni inferencia.
    int textureMode = 0;   // 0: automática; 1: grano; 2: rayas; 3: puntos.
    int paletteIndex = 0;
};
