#pragma once
#include "ofxLocalService.h"
#include "ofxVisionImage.h"

// Transporte común. La decisión artística pertenece a cada ofApp.cpp.
class ofxLiveVision
{
public:
    ofxLocalService service;
    ofEvent<ofJson> response;
    ofVideoGrabber camera;
    ofImage analyzedImage;
    ofJson result;
    float roundTripMs = 0;
    float inferenceMs = 0;
    float targetFps = 15;
    bool mirrored = true;

    void setup()
    {
        service.setup();
        ofAddListener(service.client.response, this, &ofxLiveVision::received);
        session = ofToString(ofGetSystemTimeMicros());
    }

    void connect()
    {
        if (!camera.isInitialized())
        {
            int index = 0;
            if (ofFile::doesFileExist(ofToDataPath("camera.json")))
            {
                index = ofLoadJson("camera.json").value("device", 0);
            }
            camera.setDeviceID(index);
            camera.setDesiredFrameRate(30);
            camera.setup(640, 480);
        }
        service.connect();
    }

    void update()
    {
        service.update();
        camera.update();
        if (service.ready && !camera.isInitialized())
        {
            service.status = "Camara no disponible. Revisar permisos / camera.json y pulsar C.";
        }
    }

    bool readyForFrame() const
    {
        return service.ready && camera.isFrameNew() && !service.client.busy() &&
               ofGetElapsedTimef() - sentAt >= 1.f / std::max(1.f, targetFps);
    }

    // CLASE TRANSPORTE: preparar una copia pequeña de la cámara y enviar JSON.
    // JPEG/base64 codifica la imagen; HTTP es el transporte, JSON organiza los campos.
    void sendFrame(const std::string& kind)
    {
        // Una única solicitud en vuelo: el siguiente envío usa el cuadro actual.
        // No apilamos imágenes viejas mientras el modelo calcula.
        if (!readyForFrame())
        {
            return;
        }
        pendingPixels = ofxVisionResize(camera.getPixels(), 640);
        if (mirrored)
        {
            pendingPixels.mirror(false, true);
        }
        sentAt = ofGetElapsedTimef();
        if (result.is_null())
        {
            service.status = "Cargando modelo / la primera inferencia puede demorar...";
        }
        // Esta es la llamada HTTP real compartida por los cuatro ejemplos de cámara.
        // session distingue al cliente y sequence identifica el pedido de cuadro.
        service.client.request("/vision", {{"kind", kind},
                                           {"image", ofxVisionJpeg(pendingPixels)},
                                           {"session", session},
                                           {"sequence", ++sequence}});
    }

    bool fresh() const
    {
        // Retirar resultados viejos evita dejar un cuerpo "fantasma" en la obra.
        return !result.is_null() && ofGetElapsedTimef() - captureAt < 1.0f;
    }

    void drawCamera(const ofRectangle& area)
    {
        // Dibujar la imagen exacta que produjo los puntos evita desalinear cajas.
        if (analyzedImage.isAllocated())
        {
            ofSetColor(255);
            analyzedImage.draw(area);
        }
        else if (camera.isInitialized())
        {
            ofSetColor(255);
            ofPushMatrix();
            ofTranslate(area.x + (mirrored ? area.width : 0), area.y);
            ofScale(mirrored ? -1 : 1, 1);
            camera.draw(0, 0, area.width, area.height);
            ofPopMatrix();
        }
        else
        {
            ofSetColor(25, 38, 60);
            ofDrawRectangle(area);
        }
    }

    void close()
    {
        ofRemoveListener(service.client.response, this, &ofxLiveVision::received);
        service.close();
        camera.close();
    }

private:
    ofPixels pendingPixels;
    std::string session;
    int sequence = 0;
    float sentAt = -100;
    float captureAt = -100;

    // CLASE ENTREGA: desempaquetar body y publicar el resultado a los listeners.
    // Así el onResult() de cada ejemplo recibe datos ya asociados a su imagen.
    void received(ofJson& event)
    {
        if (event.value("request", "") != "custom")
        {
            return;
        }
        const auto& body = event["body"];
        if (!body.contains("landmarks") || !body.contains("objects"))
        {
            return;
        }
        result = body;
        service.status = body.value("model", "Modelo") + " / respuesta recibida";
        captureAt = sentAt;
        roundTripMs = (ofGetElapsedTimef() - sentAt) * 1000;
        inferenceMs = body.value("inference_ms", 0.f);
        analyzedImage.setFromPixels(pendingPixels);
        ofNotifyEvent(response, result, this); // Siempre en el hilo principal de OF.
    }
};
