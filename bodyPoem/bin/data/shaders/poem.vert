#version 120

// OF 0.12.1 / ventana OpenGL 2.1 del ejemplo. Coordenadas de textura en píxeles.
varying vec2 pixel;

void main()
{
    pixel = gl_MultiTexCoord0.xy;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
