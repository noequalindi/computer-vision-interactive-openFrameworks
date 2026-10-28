#version 120
#extension GL_ARB_texture_rectangle : enable

// Los uniforms vienen de drawPoem() en C++. El shader se evalúa por fragmento.
// letterMask recorta letras; openness cambia la mezcla; time anima los patrones.
uniform sampler2DRect letterMask;
uniform vec2 resolution;
uniform float time;
uniform float openness;
uniform int patternMode;
uniform vec3 coldColor;
uniform vec3 warmColor;
varying vec2 pixel;

float grainAt(vec2 position)
{
    return fract(sin(dot(position, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    // 1. Usar el alfa de la tipografía como recorte. No pintar fuera de las letras.
    float alpha = texture2DRect(letterMask, pixel).a;
    if (alpha < 0.01)
    {
        discard;
    }

    // 2. Cambiar color con el gesto, sumando un gradiente suave dentro de cada palabra.
    vec2 uv = pixel / resolution;
    float colorMix = clamp(openness * 0.8 + uv.x * 0.25
        + sin(uv.y * 4.0 + time * 0.35) * 0.08, 0.0, 1.0);
    vec3 tint = mix(coldColor, warmColor, colorMix);

    // 3. Tres materiales procedurales: grano, rayas diagonales y trama de puntos.
    float grain = 0.80 + 0.20 * grainAt(floor(pixel * 0.65));
    float stripes = mix(0.42, 1.05,
        smoothstep(-0.12, 0.12, sin(pixel.x * 0.16 + pixel.y * 0.23 - time * 1.6)));
    vec2 cell = fract((pixel + vec2(time * 4.0, 0.0)) / 13.0) - 0.5;
    float dots = mix(0.32, 1.0, smoothstep(0.19, 0.26, length(cell)));

    float surface = mix(grain, stripes, smoothstep(0.15, 0.55, openness));
    surface = mix(surface, dots, smoothstep(0.60, 0.95, openness));
    if (patternMode == 1) surface = grain;
    if (patternMode == 2) surface = stripes;
    if (patternMode == 3) surface = dots;

    gl_FragColor = vec4(tint * surface, alpha);
}
