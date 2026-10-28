#pragma once
#include "ofMain.h"

// Dibujo y coordenadas separados del modelo; se puede reemplazar este archivo.
inline glm::vec2 visionPoint(const ofJson& landmark, const ofRectangle& area)
{
    return {area.x + landmark.value("x", 0.f) * area.width,
            area.y + landmark.value("y", 0.f) * area.height};
}

inline bool visiblePoint(const ofJson& points, int index)
{
    return points.is_array() && index < points.size()
        && points[index].value("visibility", 0.f) > 0.6f;
}

inline void drawSkeleton(const ofJson& points, const ofRectangle& area)
{
    const int bones[][2] = {{11,12},{11,13},{13,15},{12,14},{14,16},{11,23},
        {12,24},{23,24},{23,25},{25,27},{24,26},{26,28},{27,31},{28,32}};
    ofSetColor(110, 205, 255);
    ofSetLineWidth(3);
    for (const auto& bone : bones)
    {
        if (visiblePoint(points, bone[0]) && visiblePoint(points, bone[1]))
            ofDrawLine(visionPoint(points[bone[0]], area), visionPoint(points[bone[1]], area));
    }
    for (int i = 0; i < points.size(); ++i)
    {
        if (visiblePoint(points, i)) ofDrawCircle(visionPoint(points[i], area), 4);
    }
}

inline void drawConnect(const std::string& status)
{
    ofSetColor(35, 70, 125);
    ofDrawRectRounded(980, 22, 200, 42, 8);
    ofSetColor(255);
    ofDrawBitmapString("CONECTAR / C", 1015, 48);
    ofSetColor(170, 190, 220);
    ofDrawBitmapString(status.substr(0, 140), 24, 760);
}

inline bool connectClicked(int x, int y)
{
    return ofRectangle(980, 22, 200, 42).inside(x, y);
}
