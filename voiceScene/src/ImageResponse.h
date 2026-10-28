#pragma once
#include "ofMain.h"

// El servidor devuelve bytes JPEG en base64, no una ruta de su computadora.
// El límite evita que una respuesta accidentalmente enorme consuma toda la RAM.
inline ofBuffer decodeImageResponse(const std::string& encoded)
{
    if (encoded.size() > 900000)
    {
        throw std::runtime_error("Imagen demasiado grande");
    }
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string bytes;
    uint32_t bits = 0;
    int count = 0;
    for (unsigned char character : encoded)
    {
        if (character == '=')
        {
            break;
        }
        auto value = alphabet.find(character);
        if (value == std::string::npos)
        {
            throw std::runtime_error("Imagen base64 inválida");
        }
        // Cada carácter base64 aporta 6 bits. Agruparlos en bytes recupera el JPEG.
        // Esto decodifica el transporte; ofImage.load interpreta el formato de imagen.
        bits = (bits << 6) | uint32_t(value);
        count += 6;
        if (count >= 8)
        {
            count -= 8;
            bytes += char((bits >> count) & 255);
        }
    }
    return ofBuffer(bytes.data(), bytes.size());
}
