#version 410

in vec2 fTexCoord0;

uniform sampler2D Diffuse0;

out vec3 FragColor;

void main()
{
    FragColor = texture(Diffuse0, fTexCoord0).rgb;
}