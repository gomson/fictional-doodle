#version 410

in vec2 fTexCoord0;
in vec3 fNormal;
in vec3 fLight;

uniform sampler2D Diffuse0;

const float kA = 0.05;
const float kD = 0.5;

out vec3 FragColor;

void main()
{
    vec3 texColor = texture(Diffuse0, fTexCoord0).rgb;

    // Normalize interpolated inputs.
    vec3 normalDirection = normalize(fNormal);
    vec3 lightDirection = normalize(fLight);

    // Compute ambient component.
    vec3 ambient = kA * texColor;

    // Compute diffuse component.
    float lambertian = dot(normalDirection, lightDirection);
    vec3 diffuse = kD * max(lambertian, 0) * texColor;

    FragColor = ambient + diffuse;
}
