#pragma once
#include "ofMain.h"
#include <atomic>
#include <mutex>
#include <thread>

// CONEXIÓN REUTILIZABLE: llamar setup() en setup, chat() al enviar y update()
// en cada frame. Escuchar response/error para recibir el resultado.
// Todos los métodos públicos se llaman desde el hilo principal de OF.
// Solo HTTP corre en un worker. false significa ocupado/no iniciado; no encola.
class ofxLocalAI {
public:
    ofxLocalAI();
    ~ofxLocalAI();
    void setup(const std::string& url = "http://127.0.0.1:11434");
    bool chat(const ofJson& request);
    bool listModels();
    // Cliente del servidor didáctico ViT (setup con URL 8768). No consulta Ollama.
    bool classifyImage(const std::string& jpegBase64);
    // Extensión reutilizable: HTTP JSON asíncrono para servicios propios.
    bool request(const std::string& path, const ofJson& body);
    bool health();
    void update();
    void close();
    bool busy() const { return inFlight; }
    ofEvent<ofJson> response; // {request: chat|models|classification, body: respuesta del servicio}
    ofEvent<std::string> error;
private:
    bool start(const std::string& path, const std::string& kind, const ofJson* body);
    std::string baseUrl;
    bool active = false, inFlight = false, completed = false;
    std::atomic<bool> stopping{false};
    std::thread worker;
    std::mutex mutex;
    ofJson result;
    std::string failure;
};
