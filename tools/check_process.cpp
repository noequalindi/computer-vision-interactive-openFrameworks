#include "ofxLocalPython.h"
#include <chrono>
#include <iostream>
#include <thread>

// Comprueba arranque y cierre del servicio real sin webcam ni ventana OF.
int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    ofxLocalPython child;
    if (!child.start(std::filesystem::u8path(argv[1]), std::filesystem::u8path(argv[2]), {"serve"}))
    {
        std::cerr << child.error();
        return 1;
    }
    bool ready = false;
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        child.update();
        if (!child.running()) break;
        if (child.logTail().find("Uvicorn running on") != std::string::npos)
        {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    child.close();
    if (!ready || child.running() || child.exitCode() != 0)
    {
        std::cerr << child.logTail();
        return 1;
    }
    std::cout << "Servidor local: inicio y cierre limpio OK\n";
}
