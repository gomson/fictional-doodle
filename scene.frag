#version 410

in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fTangent;
in vec3 fBitangent;

uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;

out vec3 FragColor;

void main()
{
    vec3 diffuseMap = texture(DiffuseTexture, fTexCoord).rgb;
    vec3 specularMap = texture(SpecularTexture, fTexCoord).rgb;
    vec3 normalMap = texture(NormalTexture, fTexCoord).rgb;

    vec3 tangent = normalize(fTangent);
    vec3 bitangent = normalize(fBitangent);
    vec3 normal = normalize(fNormal);
    mat3 tangentModelMatrix = mat3(tangent, bitangent, normal);

    vec3 modelNormal = tangentModelMatrix * normalMap;

    FragColor = modelNormal;
}
