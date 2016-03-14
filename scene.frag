#version 410

in vec2 fTexCoord0;

out vec3 FragColor;

void main()
{
    FragColor = vec3(fTexCoord0, 0);
}