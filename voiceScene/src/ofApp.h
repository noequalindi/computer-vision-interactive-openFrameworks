#pragma once
#include "ofMain.h"
#include "ofxLocalService.h"
#include "ofxVisionDraw.h"

// Obra conversacional: la comunicación y el dibujo se pueden modificar por separado.
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
    void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
    void onResult(ofJson& result);
    void onError(std::string& message);

private:
    // Estado interno: cambiarlo en update/onResult y representarlo en draw.
    // Cliente HTTP y arranque del Python local; update() entrega sus eventos.
    ofxLocalService service;
    ofTrueTypeFont font;
    // Última imagen decodificada, reutilizable como textura de la obra.
    ofImage generatedImage;
    // session: historial; job: turno en curso; imageJob: evita cargar dos veces el JPEG.
    std::string session, job, imageJob;
    // Prompt que escribe la persona; enviar no equivale a dibujar inmediatamente.
    std::string input;
    std::string heard = "Escribí o activá ESCUCHAR para empezar.";
    std::string reply = "Podemos conversar e imaginar una imagen juntos.";
    std::string imagePrompt;
    std::string phase = "Listo";
    // IDs de voces del SO. Cadena vacía selecciona la preferencia automática.
    std::string selectedVoice, defaultVoice;
    ofJson voiceCatalog = ofJson::array();
    int voicePage = 0;
    bool voiceMenu = false, needVoices = false, voicesLoaded = false;
    // Parámetro propuesto por el LLM; OF decide aplicarlo al marco.
    ofColor color{65, 110, 170};
    // Parámetro artístico 0..1; no una medición psicológica del usuario.
    float energy = 0.25f;
    // Relojes de consulta/escucha y cantidad de caracteres visibles.
    float nextPoll = 0, nextListen = 0, reveal = 0;
    int replyScroll = 0, replyLines = 0;
    // Opciones del próximo turno: TTS, generación de imagen y conversación por turnos.
    bool speak = true, images = true, automatic = false;
    // Foco de teclado y bloqueo para evitar dos turnos superpuestos.
    bool editing = true, turnActive = false;

    void send(bool microphone);
    void newConversation();
    void refreshVoices();
    void previewVoice();
    void selectVoice(const std::string& id);
    void drawVoiceMenu();
    int drawWrapped(const std::string& value, float x, float y, float width, int lines,
                    int skip = 0);
    void button(const ofRectangle& area, const std::string& label, bool active = false);
};
