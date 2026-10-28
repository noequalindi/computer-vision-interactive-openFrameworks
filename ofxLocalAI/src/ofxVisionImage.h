#pragma once
#include "ofMain.h"

// JPEG viaja como base64 dentro de JSON. Base64 es codificación, no cifrado.
// Ollama REST espera los bytes codificados, no la ruta del archivo del alumno.
inline std::string ofxVisionBase64(const ofBuffer& bytes) {
    static const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    const auto* data=reinterpret_cast<const unsigned char*>(bytes.getData());
    const auto n=bytes.size();
    result.reserve(((n+2)/3)*4);
    for (size_t i=0;i<n;i+=3) {
        const unsigned a=data[i],b=i+1<n?data[i+1]:0,c=i+2<n?data[i+2]:0;
        result+=alphabet[a>>2];result+=alphabet[((a&3)<<4)|(b>>4)];
        result+=i+1<n?alphabet[((b&15)<<2)|(c>>6)]:'=';
        result+=i+2<n?alphabet[c&63]:'=';
    }
    return result;
}
inline std::string ofxVisionJpeg(const ofPixels& pixels) {
    ofBuffer buffer;
    if (!ofSaveImage(pixels,buffer,OF_IMAGE_FORMAT_JPEG,OF_IMAGE_QUALITY_HIGH))
        throw std::runtime_error("No se pudo codificar la imagen");
    return ofxVisionBase64(buffer);
}
// Redimensionar ANTES de enviar limita la latencia y la memoria del modelo.
inline ofPixels ofxVisionResize(const ofPixels& pixels,int longestSide=768) {
    ofPixels result=pixels;
    result.setImageType(OF_IMAGE_COLOR);
    const float ratio=std::min(1.f,float(longestSide)/std::max(result.getWidth(),result.getHeight()));
    if (ratio<1) result.resize(std::max(1,int(result.getWidth()*ratio)),std::max(1,int(result.getHeight()*ratio)));
    return result;
}
