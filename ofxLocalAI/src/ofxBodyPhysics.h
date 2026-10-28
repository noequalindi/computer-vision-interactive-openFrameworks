#pragma once
#include "ofMain.h"
#include "ofxBox2d.h"
#include "ofxVisionDraw.h"

// Extensión opcional: incluir este archivo requiere ofxBox2d en addons.make.
// El modelo entrega geometría; aquí decidimos gravedad, materiales y colisiones.
// Todo se ejecuta en el hilo de OF, nunca dentro del hilo HTTP.
class ofxBodyPhysics
{
public:
    void setup(const ofRectangle& area)
    {
        bounds = area;
        // CLASE FISICA-GRAVEDAD: aceleración vertical del mundo simulado.
        // El eje Y de esta escena crece hacia abajo; este valor no viene del modelo.
        world = std::make_unique<b2World>(b2Vec2(0, 9.8f));
        balls.clear();
        collider = nullptr;
        segments.clear();
        silhouettes.clear();
        b2BodyDef definition;
        auto walls = world->CreateBody(&definition);
        addEdge(walls, {area.x, area.y}, {area.x, area.getBottom()});
        addEdge(walls, {area.x, area.getBottom()}, {area.getRight(), area.getBottom()});
        addEdge(walls, {area.getRight(), area.getBottom()}, {area.getRight(), area.y});
        reset();
    }

    void reset()
    {
        for (const auto& ball : balls)
        {
            world->DestroyBody(ball.body);
        }
        balls.clear();
        accumulator = 0;
        rainClock = 0;
    }

    void clearBody()
    {
        if (collider)
        {
            world->DestroyBody(collider);
            collider = nullptr;
        }
        segments.clear();
        silhouettes.clear();
    }

    // Puntos normalizados -> segmentos de brazos, hombros y torso en píxeles.
    void setPose(const ofJson& points)
    {
        clearBody();
        b2BodyDef definition;
        collider = world->CreateBody(&definition);
        const int bones[][2] = {{11, 12}, {11, 13}, {13, 15}, {12, 14},
                                {14, 16}, {11, 23}, {12, 24}, {23, 24}};
        for (const auto& bone : bones)
        {
            if (!visiblePoint(points, bone[0]) || !visiblePoint(points, bone[1]))
            {
                continue;
            }
            glm::vec2 a = visionPoint(points[bone[0]], bounds);
            glm::vec2 b = visionPoint(points[bone[1]], bounds);
            if (glm::distance(a, b) < 2)
            {
                continue;
            }
            segments.push_back({a, b});
            // Una cápsula aproximada: rectángulo fino + extremos redondos.
            glm::vec2 middle = (a + b) * 0.5f;
            b2PolygonShape shape;
            shape.SetAsBox(glm::distance(a, b) / (2 * scale), bodyRadius / scale, meters(middle),
                           std::atan2(b.y - a.y, b.x - a.x));
            collider->CreateFixture(&shape, 0);
            for (const auto& point : {a, b})
            {
                b2CircleShape tip;
                tip.m_p = meters(point);
                tip.m_radius = bodyRadius / scale;
                collider->CreateFixture(&tip, 0);
            }
        }
        resolveOverlaps();
    }

    // El contorno es cerrado; no se triangula una figura cóncava como un polígono
    // convexo de Box2D. Creamos aristas y tratamos su interior como ocupado.
    void setContours(const std::vector<ofPolyline>& contours)
    {
        clearBody();
        silhouettes = contours;
        b2BodyDef definition;
        collider = world->CreateBody(&definition);
        for (const auto& contour : silhouettes)
        {
            for (std::size_t i = 0; i < contour.size(); ++i)
            {
                glm::vec2 a = contour[i];
                glm::vec2 b = contour[(i + 1) % contour.size()];
                if (glm::distance(a, b) > 0.5f)
                {
                    addEdge(collider, a, b);
                }
            }
        }
        resolveOverlaps();
    }

    void spawn()
    {
        // Límite deliberado: una clase puede dejar la obra abierta sin acumular cuerpos.
        if (balls.size() >= 60)
        {
            world->DestroyBody(balls.front().body);
            balls.erase(balls.begin());
        }
        float radius = ofRandom(10, 19);
        b2BodyDef definition;
        definition.type = b2_dynamicBody;
        definition.bullet = true;
        definition.position =
            meters({ofRandom(bounds.x + 30, bounds.getRight() - 30), bounds.y + 25});
        auto body = world->CreateBody(&definition);
        b2CircleShape circle;
        circle.m_radius = radius / scale;
        b2FixtureDef fixture;
        fixture.shape = &circle;
        fixture.density = 1;
        fixture.friction = 0.4f;
        // CLASE FISICA-MATERIAL: densidad, fricción y restitución pertenecen a la obra.
        // Restitución controla rebote, no la precisión de la detección.
        fixture.restitution = 0.45f; // VARIANTE: 0 = poco rebote; 0.8 = más rebote.
        body->CreateFixture(&fixture);
        ofColor color;
        color.setHsb(ofRandom(125, 200), 145, 255);
        balls.push_back({body, radius, color});
        resolveOverlaps();
    }

    void update(float delta, bool rain = true)
    {
        // Tiempo físico fijo: una inferencia lenta no cambia la gravedad.
        delta = ofClamp(delta, 0, 0.1f);
        rainClock += delta;
        if (rain && rainClock >= 0.4f)
        {
            spawn();
            rainClock = 0;
        }
        accumulator += delta;
        while (accumulator >= 1.0f / 60)
        {
            world->Step(1.0f / 60, 8, 3);
            accumulator -= 1.0f / 60;
        }
        for (auto it = balls.begin(); it != balls.end();)
        {
            const auto p = pixels(it->body->GetPosition());
            if (!bounds.inside(p) && (p.y > bounds.getBottom() + 80 || p.x < bounds.x - 80 ||
                                      p.x > bounds.getRight() + 80 || p.y < bounds.y - 100))
            {
                world->DestroyBody(it->body);
                it = balls.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void draw() const
    {
        ofPushStyle();
        ofFill();
        for (const auto& ball : balls)
        {
            auto position = pixels(ball.body->GetPosition());
            ofSetColor(ball.color);
            // CLASE FISICA-DIBUJO: reemplazar por un signo o sprite conserva el collider circular.
            // Cambiar sólo lo dibujado no cambia la forma física de la esfera.
            ofDrawCircle(position, ball.radius);
            ofSetColor(255, 150);
            ofDrawCircle(position + glm::vec2(-ball.radius * 0.25f), ball.radius * 0.22f);
        }
        ofNoFill();
        ofSetColor(95, 130, 185);
        ofDrawRectangle(bounds);
        ofPopStyle();
    }

    std::size_t count() const
    {
        return balls.size();
    }

private:
    static constexpr float scale = 30;
    static constexpr float bodyRadius = 6;
    struct Ball
    {
        b2Body* body;
        float radius;
        ofColor color;
    };
    struct Segment
    {
        glm::vec2 a, b;
    };
    std::unique_ptr<b2World> world;
    b2Body* collider = nullptr;
    std::vector<Ball> balls;
    std::vector<Segment> segments;
    std::vector<ofPolyline> silhouettes;
    ofRectangle bounds;
    float accumulator = 0;
    float rainClock = 0;

    static b2Vec2 meters(glm::vec2 p)
    {
        return {p.x / scale, p.y / scale};
    }
    static glm::vec2 pixels(b2Vec2 p)
    {
        return {p.x * scale, p.y * scale};
    }

    void addEdge(b2Body* body, glm::vec2 a, glm::vec2 b)
    {
        b2EdgeShape edge;
        edge.Set(meters(a), meters(b));
        body->CreateFixture(&edge, 0);
    }

    void moveOutside(Ball& ball, glm::vec2 boundary, glm::vec2 direction, float padding)
    {
        if (glm::length(direction) < 0.001f)
        {
            direction = {0, -1};
        }
        direction = glm::normalize(direction);
        auto target = boundary + direction * (ball.radius + padding + 1);
        ball.body->SetTransform(meters(target), ball.body->GetAngle());
        // Empuje limitado, no una medición física de fuerza humana.
        auto velocity = ball.body->GetLinearVelocity();
        auto normal = b2Vec2(direction.x, direction.y);
        float incoming = b2Dot(velocity, normal);
        if (incoming < 0)
        {
            velocity -= incoming * normal;
        }
        ball.body->SetLinearVelocity(velocity + 0.4f * normal);
        ball.body->SetAwake(true);
    }

    void resolveOverlaps()
    {
        // La cámara entrega geometría discreta: si una nueva silueta aparece
        // encima de una esfera, la sacamos del interior antes del siguiente Step.
        // Es una aproximación 2D para interacción, no profundidad de Kinect.
        for (auto& ball : balls)
        {
            for (const auto& segment : segments)
            {
                glm::vec2 p = pixels(ball.body->GetPosition());
                glm::vec2 ab = segment.b - segment.a;
                float t = ofClamp(glm::dot(p - segment.a, ab) / glm::dot(ab, ab), 0, 1);
                glm::vec2 closest = segment.a + t * ab;
                if (glm::distance(p, closest) < ball.radius + bodyRadius)
                {
                    moveOutside(ball, closest, p - closest, bodyRadius);
                }
            }
            for (const auto& contour : silhouettes)
            {
                if (contour.size() < 3)
                {
                    continue;
                }
                glm::vec2 p = pixels(ball.body->GetPosition());
                glm::vec2 closest = contour.getClosestPoint(glm::vec3(p, 0));
                bool inside = contour.inside(p.x, p.y);
                if (inside || glm::distance(p, closest) < ball.radius)
                {
                    moveOutside(ball, closest, inside ? closest - p : p - closest, 0);
                }
            }
        }
    }
};
