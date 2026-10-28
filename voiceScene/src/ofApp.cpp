// GUÍA DE CLASE: Texto o voz -> conversación local -> texto, imagen y ambientación en OF.
// Prácticas y recorrido del código: docs/GUIA_CODIGO_OF.md en el repositorio.

#include "ofApp.h"
#include "ImageResponse.h"
#include <sstream>

// PREPARAR: crear la conexión, registrar respuestas/errores y recuperar la voz
// guardada. OF escribe y dibuja; Python coordina micrófono, modelos y reproducción.
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofSetEscapeQuitsApp(false);
    font.load(OF_TTF_SANS, 14, true, true);
    service.setup();
    ofAddListener(service.client.response, this, &ofApp::onResult);
    ofAddListener(service.client.error, this, &ofApp::onError);
    newConversation();
    try
    {
        const auto path = ofToDataPath("voice-settings.json", true);
        if (ofFile::doesFileExist(path))
        {
            selectedVoice = ofLoadJson(path).value("voice", "");
        }
    }
    catch (const std::exception&)
    {
        phase = "No se pudo leer la voz guardada; se usará Automática.";
    }
}

// MEMORIA: una sesión nueva separa el historial en Python.
// Limpiamos la interfaz local; no se modifican ni reentrenan pesos.
void ofApp::newConversation()
{
    if (turnActive)
    {
        return;
    }
    // La sesión identifica el historial. Una nueva sesión empieza sin recuerdos.
    session = ofToString(ofGetSystemTimeMillis()) + "-" + ofToString(ofRandom(1000000));
    job.clear();
    imageJob.clear();
    generatedImage.clear();
    imagePrompt.clear();
    input.clear();
    heard = "Escribí o activá ESCUCHAR para empezar.";
    reply = "Podemos conversar e imaginar una imagen juntos.";
    phase = "Conversación nueva";
    replyScroll = 0;
    reveal = 0;
    automatic = false;
}

// ACTUALIZAR: OF repite esta función mientras la ventana está abierta.
// Recibimos datos y avanzamos la obra; no esperamos una inferencia de forma bloqueante.
void ofApp::update()
{
    service.update(); // Las respuestas se entregan en el hilo principal, sin frenar draw().
    reveal += ofGetLastFrameTime() * 42; // Revelado visual, no streaming de tokens del LLM.

    if (needVoices && service.ready && !turnActive && !service.client.busy())
    {
        refreshVoices();
        return;
    }

    if (turnActive && !job.empty() && !service.client.busy() && ofGetElapsedTimef() >= nextPoll)
    {
        // [2] Consultar el progreso: escuchar, pensar, dibujar, hablar o terminar.
        // Cada petición dura poco; la generación puede continuar varios minutos.
        service.client.request("/conversation/poll", {{"job", job}, {"session", session}});
        nextPoll = ofGetElapsedTimef() + 0.4f;
    }

    if (automatic && !voiceMenu && !turnActive && service.ready && !service.client.busy() &&
        ofGetElapsedTimef() >= nextListen)
    {
        send(true); // AUTO: turnos alternados. No escucha mientras el TTS está hablando.
    }
}

// ENVIAR UN TURNO: reúne texto u orden de escuchar y opciones en un JSON.
// El bool microphone elige la entrada; el resultado llegará después por eventos.
void ofApp::send(bool microphone)
{
    if (!service.ready || turnActive || service.client.busy())
    {
        return;
    }
    if (!microphone && input.empty())
    {
        return;
    }

    // [1] ESTE ES EL PEDIDO. El prompt es texto libre, no un nombre de escena.
    // Python captura el micrófono si microphone=true; OF no envía un WAV.
    // CLASE VOZ-ENVIO: session conserva contexto; text lleva el prompt.
    // speak/images habilitan herramientas; voice identifica el timbre del TTS.
    const ofJson request = {{"session", session}, {"text", input},    {"microphone", microphone},
                            {"speak", speak},     {"images", images}, {"voice", selectedVoice}};
    if (service.client.request("/conversation/start", request))
    {
        turnActive = true;
        job.clear();
        reply.clear();
        reveal = 0;
        heard = microphone ? "Preparando micrófono..." : input;
        if (!microphone)
        {
            input.clear();
        }
        phase = "Preparando turno...";
    }
}

// RECIBIR: primero distinguir salud, catálogo de voces y trabajo conversacional.
// El primer resultado identifica el trabajo; los siguientes aportan progreso y salidas.
void ofApp::onResult(ofJson& event)
{
    if (event.value("request", "") == "health")
    {
        if (event["body"].value("voice_protocol", 0) < 3)
        {
            service.ready = false;
            service.status =
                "Servidor anterior: cerrarlo y volver a CONECTAR con el Python actualizado.";
        }
        else
        {
            needVoices = true;
        }
        return;
    }
    if (event.value("request", "") != "custom")
    {
        return;
    }
    const auto& result = event["body"];
    if (result.value("kind", "") == "voices")
    {
        voiceCatalog = result.at("voices");
        voicesLoaded = true;
        defaultVoice = result.value("default_voice", "");
        voicePage = 0;
        bool available = selectedVoice.empty();
        for (const auto& voice : voiceCatalog)
        {
            available = available || voice.value("id", "") == selectedVoice;
        }
        if (!available)
        {
            selectVoice("");
            phase = "La voz guardada ya no está instalada; se seleccionó Automática.";
        }
        else
        {
            phase = result.value("notice", "").empty()
                        ? "Voces disponibles. Elegí una o probá Automática."
                        : result["notice"].get<std::string>();
        }
        return;
    }
    // La respuesta inicial de /start o /voice/preview trae job.
    // Guardarlo permite que update consulte /poll sin reenviar el prompt.
    if (job.empty())
    {
        job = result.at("job").get<std::string>();
        return;
    }

    // CLASE VOZ-RESPUESTA: message es estado de interfaz; result contiene las salidas.
    // No asumir que ya hay imagen o reply: los campos aparecen durante el trabajo.
    phase = result.value("message", "Esperando...");
    const auto& data = result.at("result");
    if (data.contains("text"))
    {
        heard = data["text"].get<std::string>();
    }
    if (data.contains("reply") && reply != data["reply"].get<std::string>())
    {
        reply = data["reply"].get<std::string>();
        replyScroll = 0;
        reveal = 0;
    }
    if (data.contains("energy"))
    {
        // [3] HACER ALGO CON LA RESPUESTA. El LLM propone datos; OF decide el uso.
        // VARIANTE V1 / Elegir UNA línea: acompañar o contradecir el ritmo.
        // CLASE VOZ-ACCION: conservar un parámetro para usarlo luego en draw().
        energy = data["energy"].get<float>();
        // energy = 1.f - data["energy"].get<float>();
        color = ofColor(data["color"][0].get<int>(), data["color"][1].get<int>(),
                        data["color"][2].get<int>());
    }
    if (data.contains("image_prompt"))
    {
        imagePrompt = data["image_prompt"].get<std::string>();
    }
    if (data.contains("image_base64") && imageJob != job)
    {
        try
        {
            // [4] La imagen la produjo Stable Diffusion. Cargarla permite dibujar,
            // recortar, deformar o usar sus píxeles/textura en la instalación.
            ofImage next;
            if (!next.load(decodeImageResponse(data["image_base64"].get<std::string>())))
            {
                throw std::runtime_error("No se pudo decodificar la imagen generada");
            }
            generatedImage = next;
            imageJob = job;
        }
        catch (const std::exception& error)
        {
            phase = error.what();
        }
    }

    const auto state = result.value("state", "");
    if (state == "done" || state == "failed")
    {
        turnActive = false;
        nextListen = ofGetElapsedTimef() + 1.5f;
        // Evitar bucles automáticos ante permisos, modelos o micrófono fallidos.
        if (state == "failed" || !data.value("recognized", false))
        {
            automatic = false;
        }
    }
}

// ERROR RECUPERABLE: liberar el turno y detener AUTO para no repetir solicitudes
// fallidas. El mensaje se muestra en pantalla; la app sigue dibujando.
void ofApp::onError(std::string& message)
{
    turnActive = false;
    automatic = false;
    phase = message;
}

// MAQUETAR TEXTO: medir palabras con la fuente, repartir renglones y aplicar scroll.
// Es una utilidad gráfica; no tokeniza para el LLM ni hace una consulta al modelo.
int ofApp::drawWrapped(const std::string& value, float x, float y, float width, int count, int skip)
{
    // Cortar entre palabras conserva la lectura; el scroll permite leer respuestas largas.
    std::vector<std::string> lines;
    for (const auto& paragraph : ofSplitString(value, "\n", false, false))
    {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word)
        {
            const auto candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && font.stringWidth(candidate) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else
            {
                line = candidate;
            }
        }
        lines.push_back(line);
    }
    for (int row = 0; row < count && row + skip < int(lines.size()); ++row)
    {
        font.drawString(lines[row + skip], x, y + row * 23);
    }
    return int(lines.size());
}

// DIBUJAR BOTÓN: el estado active cambia el color. La detección del clic ocurre
// en mousePressed(); dibujar un botón por sí solo no ejecuta una acción.
void ofApp::button(const ofRectangle& area, const std::string& label, bool active)
{
    ofSetColor(active ? ofColor(62, 104, 158) : ofColor(30, 48, 79));
    ofDrawRectRounded(area, 7);
    ofSetColor(240);
    font.drawString(label, area.x + 12, area.y + area.height * 0.5f + 6);
}

// DIBUJAR: representar el estado actual. Cambiar colores, tamaños o composición
// aquí modifica la obra sin cambiar los pesos del modelo.
void ofApp::draw()
{
    ofBackground(12, 20, 36);
    ofSetColor(235);
    font.drawString("UNA / INFORMÁTICA APLICADA 2", 24, 36);
    font.drawString("voiceScene · conversar, imaginar, responder", 24, 64);
    ofSetColor(20, 33, 54);
    ofDrawRectRounded(24, 104, 566, 458, 12);
    ofSetColor(150, 180, 215);
    font.drawString("VOS", 42, 135);
    ofSetColor(240);
    drawWrapped(heard, 42, 168, 526, 5);
    ofSetColor(150, 180, 215);
    font.drawString("RESPUESTA DEL LLM · SCROLL PARA LEER", 42, 305);
    ofSetColor(245);
    // La animación de escritura se hace en OF; el servidor ya entregó el texto.
    replyLines =
        drawWrapped(ofUTF8Substring(reply, 0, size_t(reveal)), 42, 340, 526, 9, replyScroll);

    ofSetColor(20, 33, 54);
    ofDrawRectRounded(610, 104, 566, 458, 12);
    if (generatedImage.isAllocated())
    {
        ofSetColor(255);
        // VARIANTE V2 / Elegir UNA línea: imagen fija o respiración según energía.
        float scale = 1.f;
        // float scale = 0.96f + 0.04f * std::sin(ofGetElapsedTimef() * (0.5f + energy * 3));
        const float side = 408 * scale;
        generatedImage.draw(893 - side / 2, 333 - side / 2, side, side);
    }
    else
    {
        ofSetColor(175, 193, 218);
        drawWrapped("Pedime una imagen: por ejemplo, una ciudad hecha de instrumentos musicales.",
                    650, 270, 476, 5);
        drawWrapped(
            "Conversá primero o pedí una imagen directamente. Activá IMAGEN para generarla.", 650,
            405, 476, 4);
    }
    // El marco es ambientación de OF. No lo confundimos con la imagen generada.
    ofNoFill();
    ofSetLineWidth(2 + energy * 3);
    ofSetColor(color);
    ofDrawRectRounded(610, 104, 566, 458, 12);
    ofFill();
    ofSetColor(170, 189, 216);
    drawWrapped(imagePrompt.empty() ? "Imagen: se conserva la última hasta que pidas otra."
                                    : "Prompt visual: " + imagePrompt,
                24, 589, 1150, 2);

    button({24, 630, 770, 50}, "", editing);
    ofSetColor(255);
    std::string tail = input;
    while (font.stringWidth(tail) > 735)
    {
        ofUTF8Erase(tail, 0, 1);
    }
    font.drawString(tail + (editing && std::fmod(ofGetElapsedTimef(), 1) < 0.5 ? "|" : ""), 36,
                    660);
    button({810, 630, 175, 50}, "ENVIAR / ENTER");
    button({1000, 630, 176, 50}, "ESCUCHAR / M");
    button({24, 696, 165, 40}, speak ? "VOZ: ON" : "VOZ: OFF", speak);
    button({204, 696, 180, 40}, images ? "IMAGEN: ON" : "IMAGEN: OFF", images);
    button({399, 696, 170, 40}, automatic ? "AUTO: ON" : "AUTO: OFF", automatic);
    button({584, 696, 190, 40}, "NUEVA CHARLA");
    ofSetColor(190, 210, 240);
    font.drawString("Esc: salir de texto", 810, 723);
    drawConnect("");
    std::string voiceLabel = selectedVoice.empty() ? "Automática" : selectedVoice;
    const std::string countLabel =
        voicesLoaded ? " (" + ofToString(voiceCatalog.size()) + ")" : " (...)";
    while (font.stringWidth("Elegir voz: " + voiceLabel + countLabel) > 342 &&
           ofUTF8Length(voiceLabel) > 1)
    {
        ofUTF8Erase(voiceLabel, ofUTF8Length(voiceLabel) - 1, 1);
    }
    button({610, 68, 400, 28}, "Elegir voz: " + voiceLabel + countLabel, voiceMenu);
    ofSetColor(220, 235, 255);
    ofDrawTriangle(985, 79, 997, 79, 991, 86); // Flecha de desplegable.
    button({1024, 68, 152, 28}, "PROBAR VOZ");
    ofSetColor(185, 207, 237);
    const auto status = service.ready ? phase : service.status;
    drawWrapped((turnActive ? std::string(int(ofGetElapsedTimef() * 2) % 4, '.') + " " : "") +
                    status,
                24, 764, 1150, 2);
    if (voiceMenu)
    {
        drawVoiceMenu();
    }
}

// ENTRADA DE TEXTO: Enter envía y Escape sale de edición. Mientras editing
// es true, las letras se agregan al prompt; fuera de edición funcionan los atajos.
void ofApp::keyPressed(int key)
{
    if (key == OF_KEY_ESC)
    {
        editing = false;
        voiceMenu = false;
        return;
    }
    if (key == OF_KEY_RETURN)
    {
        send(false);
        return;
    }
    if (editing)
    {
        if (key == OF_KEY_BACKSPACE && !input.empty())
        {
            ofUTF8Erase(input, ofUTF8Length(input) - 1, 1);
        }
        else if (key >= 32 && key < OF_KEY_F1 && input.size() < 1100)
        {
            ofUTF8Append(input, key);
        }
        return;
    }
    if (key == 'c')
    {
        service.connect();
    }
    if (key == 'v')
    {
        speak = !speak;
    }
    if (key == 'i')
    {
        images = !images;
    }
    if (key == 'm')
    {
        send(true);
    }
}

// INTERACCIÓN: primero atender el desplegable de voces, luego el resto de botones.
// Cambiar una opción no inicia un turno; ENVIAR o ESCUCHAR llaman a send().
void ofApp::mousePressed(int x, int y, int buttonIndex)
{
    if (ofRectangle(610, 68, 400, 28).inside(x, y))
    {
        editing = false;
        if (!service.ready)
        {
            service.status = "Primero pulsá CONECTAR para consultar las voces.";
            return;
        }
        if (turnActive)
        {
            phase = "Elegí la voz cuando termine este turno.";
            return;
        }
        voiceMenu = !voiceMenu;
        if (voiceMenu && voiceCatalog.empty())
        {
            needVoices = true;
        }
        return;
    }
    if (voiceMenu)
    {
        if (ofRectangle(628, 150, 530, 34).inside(x, y))
        {
            selectVoice("");
            return;
        }
        for (int row = 0; row < 8; ++row)
        {
            int index = voicePage * 8 + row;
            if (index < int(voiceCatalog.size()) &&
                ofRectangle(628, 190 + row * 34, 530, 32).inside(x, y))
            {
                selectVoice(voiceCatalog[index].value("id", ""));
                return;
            }
        }
        if (ofRectangle(628, 480, 120, 36).inside(x, y))
        {
            voicePage = std::max(0, voicePage - 1);
        }
        else if (ofRectangle(760, 480, 120, 36).inside(x, y))
        {
            voicePage = std::min(std::max(0, (int(voiceCatalog.size()) - 1) / 8), voicePage + 1);
        }
        else if (ofRectangle(996, 480, 162, 36).inside(x, y))
        {
            refreshVoices();
        }
        else if (!ofRectangle(610, 108, 566, 445).inside(x, y))
        {
            voiceMenu = false;
        }
        return;
    }
    if (ofRectangle(1024, 68, 152, 28).inside(x, y))
    {
        previewVoice();
        return;
    }
    if (connectClicked(x, y))
    {
        service.connect();
    }
    editing = ofRectangle(24, 630, 770, 50).inside(x, y);
    if (ofRectangle(810, 630, 175, 50).inside(x, y))
    {
        send(false);
    }
    if (ofRectangle(1000, 630, 176, 50).inside(x, y))
    {
        send(true);
    }
    if (ofRectangle(24, 696, 165, 40).inside(x, y))
    {
        speak = !speak;
    }
    if (ofRectangle(204, 696, 180, 40).inside(x, y))
    {
        images = !images;
    }
    if (ofRectangle(399, 696, 170, 40).inside(x, y))
    {
        automatic = !automatic;
    }
    if (ofRectangle(584, 696, 190, 40).inside(x, y))
    {
        newConversation();
    }
}

// CATÁLOGO: pedir voces instaladas al servicio. El listado llega a onResult().
// Elegir una voz disponible evita depender de nombres distintos entre sistemas.
void ofApp::refreshVoices()
{
    if (!service.ready || turnActive || service.client.busy())
    {
        return;
    }
    needVoices = !service.client.request("/voices", {{"refresh", true}});
}

// PREFERENCIA: guardar el identificador exacto de una voz para próximos pedidos.
// Afecta el sintetizador TTS, no la personalidad ni la memoria del LLM.
void ofApp::selectVoice(const std::string& id)
{
    selectedVoice = id;
    voiceMenu = false;
    // Preferencia de ESTA instalación de OF; no cambia el LLM ni su personalidad.
    try
    {
        ofSavePrettyJson(ofToDataPath("voice-settings.json", true), {{"voice", selectedVoice}});
        phase = "Voz seleccionada para las próximas respuestas.";
    }
    catch (const std::exception&)
    {
        phase = "Voz seleccionada; no se pudo guardar la preferencia en disco.";
    }
}

// PROBAR VOZ: enviar sólo una muestra TTS y consultar su trabajo como cualquier turno.
// Repite el timbre seleccionado; no avanza por el catálogo ni abre el micrófono.
void ofApp::previewVoice()
{
    if (!service.ready || turnActive || service.client.busy())
    {
        return;
    }
    automatic = false;
    // Prueba TTS sin gastar una consulta al LLM ni activar el micrófono.
    if (service.client.request("/voice/preview", {{"session", session}, {"voice", selectedVoice}}))
    {
        job.clear();
        turnActive = true;
        phase = "Preparando prueba de voz...";
    }
}

// SELECTOR: mostrar ocho voces por página y una opción automática.
// El catálogo pertenece al sistema operativo; no son ocho modelos LLM diferentes.
void ofApp::drawVoiceMenu()
{
    ofSetColor(26, 42, 67);
    ofDrawRectRounded(610, 108, 566, 445, 12);
    ofSetColor(235);
    font.drawString(ofToString(voiceCatalog.size()) + " VOCES · español primero", 628, 136);
    std::string automaticLabel =
        "Automática" + (defaultVoice.empty() ? "" : " (" + defaultVoice + ")");
    button({628, 150, 530, 34}, automaticLabel, selectedVoice.empty());
    if (voiceCatalog.empty())
    {
        ofSetColor(200, 215, 235);
        drawWrapped(voicesLoaded ? "El servidor no devolvió voces. Pulsá ACTUALIZAR."
                                 : "Cargando el catálogo de voces del servidor...",
                    640, 223, 490, 3);
        drawWrapped("Automática es una preferencia, no la lista completa de voces.", 640, 310, 490,
                    3);
    }
    for (int row = 0; row < 8; ++row)
    {
        int index = voicePage * 8 + row;
        if (index >= int(voiceCatalog.size()))
        {
            break;
        }
        const auto& voice = voiceCatalog[index];
        std::string label = voice.value("name", "") + " · " + voice.value("language", "");
        while (font.stringWidth(label) > 502 && ofUTF8Length(label) > 1)
        {
            ofUTF8Erase(label, ofUTF8Length(label) - 1, 1);
        }
        button({628, float(190 + row * 34), 530, 32}, label,
               selectedVoice == voice.value("id", ""));
    }
    button({628, 480, 120, 36}, "Anterior");
    button({760, 480, 120, 36}, "Siguiente");
    button({996, 480, 162, 36}, "ACTUALIZAR");
    ofSetColor(185, 207, 237);
    font.drawString(ofToString(voicePage + 1) + "/" +
                        ofToString(std::max(1, (int(voiceCatalog.size()) + 7) / 8)),
                    901, 505);
    font.drawString("La voz cambia el timbre; el idioma lo define el texto.", 628, 539);
}

// LEER RESPUESTAS LARGAS: mover el inicio de los renglones visibles sólo cuando
// el cursor está sobre la zona de respuesta; no se vuelve a consultar al modelo.
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    if (ofRectangle(24, 300, 566, 262).inside(x, y))
    {
        replyScroll = ofClamp(replyScroll - int(scrollY * 2), 0, std::max(0, replyLines - 9));
    }
}

// CERRAR: quitar listeners antes de destruir el receptor y liberar cámara/servicio.
// El addon sólo detiene el Python que inició; conserva un servidor iniciado aparte.
void ofApp::exit()
{
    ofRemoveListener(service.client.response, this, &ofApp::onResult);
    ofRemoveListener(service.client.error, this, &ofApp::onError);
    service.close();
}
