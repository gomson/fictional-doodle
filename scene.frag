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
uniform vec3 CameraPosition;

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

    vec3 modelPosition = fPosition;
    vec3 modelNormal = tangentModelMatrix * normalMap;
    vec3 modelCameraPos = (WorldModel * vec4(CameraPosition, 1)).xyz;

    vec3 V = normalize(modelCameraPos - modelPosition); // towards viewer
    vec3 N = modelNormal; // normal
    vec3 L = normalize(modelLightPosition - modelPosition); // light direction
    float G = max(0, dot(N, L)); // geometric term
    vec3 R = reflect(L, N); // reflection direction
    float S = pow(max(0, dot(R, V)), 5); // specular coefficient

    float kA = 0.01;
    float kD = 1.0;
    float kS = 0.2;

    vec3 ambient = vec3(1) * kA;
    vec3 diffuse = G * diffuseMap * kD;
    vec3 specular = S * specularMap * kS;

    FragColor = ambient + diffuse + specular;
}
