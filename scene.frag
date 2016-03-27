#version 410

in vec3 fPosition;
in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fTangent;
in vec3 fBitangent;

uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;

out vec4 FragColor;

uniform mat4 WorldModel;
uniform vec3 CameraPosition;
uniform int IlluminationModel;

void main()
{
    vec3 worldLightPosition = vec3(500, 400, 0);
    vec3 modelLightPosition = (WorldModel * vec4(worldLightPosition, 1)).xyz;
    
    vec3 modelCameraPos = (WorldModel * vec4(CameraPosition, 1)).xyz;

    vec3 modelPosition = fPosition;
    vec3 L = normalize(modelLightPosition - modelPosition); // light direction
    vec3 V = normalize(modelCameraPos - modelPosition); // towards viewer

    if (IlluminationModel == 1) // standard opaque material
    {
        vec4 diffuseMap = texture(DiffuseTexture, fTexCoord);
        vec4 specularMap = texture(SpecularTexture, fTexCoord);

        vec3 normalMap = normalize(texture(NormalTexture, fTexCoord).rgb);
        vec3 tangent = normalize(fTangent);
        vec3 bitangent = normalize(fBitangent);
        vec3 normal = normalize(fNormal);
        if (dot(cross(normal, tangent), bitangent) < 0)
        {
            tangent = -tangent;
        }
        mat3 tangentModelMatrix = mat3(tangent, bitangent, normal);
        vec3 modelNormal = tangentModelMatrix * normalMap;

        float a = 30;
        float kA = 0.03;
        float kD = 0.5;
        float kS = 0.7;

        vec3 N = normalize(modelNormal); // normal
        float G = max(0, dot(N, L)); // geometric term
        vec3 R = reflect(-L, N); // reflection direction
        float S = pow(max(0, dot(R, V)), a); // specular coefficient

        vec4 ambient = diffuseMap * kA;
        vec4 diffuse = G * diffuseMap * kD;
        vec4 specular = S * specularMap * kS;

        FragColor = vec4(ambient.rgb + diffuse.rgb + specular.rgb, 1.0);
    }
    else if (IlluminationModel == 2) // Transparent objects
    {
        // note: lots of hacks here specific to hellknight...
        float kD = 1.0;
        float kS = 0.1;
        vec4 diffuseMap = texture(DiffuseTexture, fTexCoord) * kD;
        vec4 specularMap = texture(SpecularTexture, fTexCoord) * kS;
        FragColor = vec4(diffuseMap.rgb + specularMap.rgb, diffuseMap.a* fTexCoord.x * fTexCoord.y);
    }
}
