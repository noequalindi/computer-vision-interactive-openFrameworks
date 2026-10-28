#pragma once
#include "ofMain.h"
#include <atomic>

// Sonificación: el modelo mide; OF sintetiza. S activa el audio por decisión del alumno.
class ofxTone : public ofBaseSoundOutput
{
public:
    std::atomic<float> frequency{220};
    std::atomic<float> volume{0};
    bool enabled = false;

    void toggle()
    {
        if (!enabled)
        {
            ofSoundStreamSettings settings;
            settings.setOutListener(this);
            settings.sampleRate = 44100;
            settings.numOutputChannels = 2;
            settings.numInputChannels = 0;
            settings.bufferSize = 256;
            enabled = stream.setup(settings);
        }
        else
        {
            stream.close();
            enabled = false;
        }
    }

    void audioOut(ofSoundBuffer& buffer) override
    {
        // Sin HTTP, memoria dinámica ni dibujo dentro del callback de audio.
        const float target = ofClamp(volume.load(), 0, 0.15f);
        const float hz = ofClamp(frequency.load(), 80, 1000);
        for (size_t i = 0; i < buffer.getNumFrames(); ++i)
        {
            amplitude += (target - amplitude) * 0.002f;
            phase += TWO_PI * hz / buffer.getSampleRate();
            if (phase > TWO_PI) phase -= TWO_PI;
            for (size_t c = 0; c < buffer.getNumChannels(); ++c)
                buffer[i * buffer.getNumChannels() + c] = std::sin(phase) * amplitude;
        }
    }

    void close() { stream.close(); }
private:
    ofSoundStream stream;
    float phase = 0;
    float amplitude = 0;
};
