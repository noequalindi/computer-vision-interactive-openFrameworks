#pragma once
#include "ofxLocalAI.h"
#include "ofxLocalPython.h"
#include <memory>

// Inicia SOLO el servidor de este kit, sin shell y sin comandos arbitrarios.
// Si ya existe un servidor compatible, se conecta y no toma propiedad de él.
class ofxLocalService
{
public:
    ofxLocalAI client;
    bool ready = false;
    std::string status = "Desconectado. Pulsar CONECTAR.";

    void setup()
    {
        client.setup("http://127.0.0.1:8770");
        ofAddListener(client.response, this, &ofxLocalService::received);
        ofAddListener(client.error, this, &ofxLocalService::failed);
    }

    void connect()
    {
        if (client.busy() || connecting || ready) return;
        connecting = true;
        triedLaunch = false;
        deadline = ofGetElapsedTimef() + 25;
        nextProbe = 0;
        status = "Conectando con Python local...";
    }

    void update()
    {
        owned.update();
        client.update();
        if (connecting && triedLaunch && !owned.running() && !owned.error().empty())
        {
            connecting = false;
            status = owned.error();
        }
        if (connecting && ofGetElapsedTimef() > deadline)
        {
            connecting = false;
            status = "Sin servicio. Revisar instalacion o ejecutar start_local.";
        }
        if (connecting && !client.busy() && ofGetElapsedTimef() >= nextProbe)
        {
            client.health();
            nextProbe = ofGetElapsedTimef() + 0.5;
        }
    }

    void close()
    {
        client.close();
        ofRemoveListener(client.response, this, &ofxLocalService::received);
        ofRemoveListener(client.error, this, &ofxLocalService::failed);
        // Nunca cerrar un servidor que el alumno ya había iniciado por terminal.
        owned.close();
    }

private:
    bool connecting = false;
    bool triedLaunch = false;
    float deadline = 0;
    float nextProbe = 0;
    ofxLocalPython owned;

    void received(ofJson& event)
    {
        if (event.value("request", "") != "health") return;
        const auto& body = event["body"];
        ready = body.value("service", "") == "una-realtime" && body.value("protocol", 0) == 1;
        connecting = false;
        status = ready ? "Conectado / CPU / HTTP local" : "Puerto 8770 ocupado por otro servicio.";
    }

    void failed(std::string& message)
    {
        status = message;
        if (!connecting) { ready = false; return; }
        if (triedLaunch) return;
        triedLaunch = true;
        try
        {
            std::string root;
            const auto configPath = ofToDataPath("local-service.json", true);
            if (ofFile::doesFileExist(configPath))
                root = ofLoadJson(configPath).value("project_root", "");
            if (root.empty() || !ofFile::doesFileExist(root + "/realtime/server.py"))
            {
                auto choice = ofSystemLoadDialog("Elegir carpeta computer-vision-interactive-openFrameworks", true);
                if (!choice.bSuccess) { connecting = false; return; }
                root = choice.getPath();
            }
#ifdef TARGET_WIN32
            const auto python = root + "/.venv/Scripts/python.exe";
#else
            const auto python = root + "/.venv/bin/python";
#endif
            if (!ofFile::doesFileExist(python) || !ofFile::doesFileExist(root + "/realtime/server.py"))
                throw std::runtime_error("Ejecutar install_env en la carpeta elegida antes de conectar.");
            // Lista de argumentos: las rutas con espacios no se interpretan como shell.
            if (!owned.start(std::filesystem::u8path(root), std::filesystem::u8path(python), {"serve"}))
                throw std::runtime_error(owned.error());
            ofSavePrettyJson(configPath, ofJson{{"project_root", root}});
            deadline = ofGetElapsedTimef() + 25;
            status = "Iniciando Python...";
        }
        catch (const std::exception& error)
        {
            connecting = false;
            status = error.what();
        }
    }
};
