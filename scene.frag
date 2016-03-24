#version 410

in vec3 fPosition;
in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fTangent;
in vec3 fBitangent;

uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;

out vec3 FragColor;

uniform mat4 WorldModel;

void main()
{
    vec3 worldLightPosition = vec3(100, 100, 100);
    vec3 modelLightPosition = (WorldModel * vec4(worldLightPosition, 1)).xyz;

    vec3 diffuseMap = texture(DiffuseTexture, fTexCoord).rgb;
    vec3 specularMap = texture(SpecularTexture, fTexCoord).rgb;
    vec3 normalMap = normalize(texture(NormalTexture, fTexCoord).rgb);

    vec3 tangent = normalize(fTangent);
    vec3 bitangent = normalize(fBitangent);
    vec3 normal = normalize(fNormal);
    mat3 tangentModelMatrix = mat3(tangent, bitangent, normal);

    vec3 modelNormal = tangentModelMatrix * normalMap;

    vec3 N = modelNormal;
    vec3 L = normalize(modelLightPosition - fPosition);
    float G = dot(N, L); // geometric term

    FragColor = diffuseMap * G;
}
