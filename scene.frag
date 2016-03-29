#version 410

in vec3 fPosition;
in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fTangent;
in vec3 fBitangent;

uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D NormalTexture;
uniform sampler2DShadow ShadowMapTexture;

out vec4 FragColor;

uniform mat4 ModelWorld;
uniform mat4 WorldModel;
uniform vec3 CameraPosition;
uniform vec3 LightPosition;
uniform mat4 WorldLightProjection;
uniform vec3 BackgroundColor;
uniform int IlluminationModel;
uniform int HasNormalMap;

void main()
{
    vec3 worldLightPosition = LightPosition;
    vec3 modelLightPosition = (WorldModel * vec4(worldLightPosition, 1)).xyz;
    
    vec3 modelCameraPos = (WorldModel * vec4(CameraPosition, 1)).xyz;

    vec3 modelPosition = fPosition;
    vec3 L = normalize(modelLightPosition - modelPosition); // light direction
    vec3 V = normalize(modelCameraPos - modelPosition); // towards viewer

    vec3 worldPosition = (ModelWorld * vec4(modelPosition, 1.0)).xyz;
    vec4 shadowMapCoord = WorldLightProjection * vec4(worldPosition, 1.0);
    shadowMapCoord.xyz *= vec3(0.5, 0.5, 0.5); // [-1,+1] -> [-0.5,+0.5]
    shadowMapCoord.xyz += shadowMapCoord.w * vec3(0.5, 0.5, 0.5); // w will divide up to 0.5 instead of 1.0
    float shadowPass = textureProj(ShadowMapTexture, shadowMapCoord);

    if (IlluminationModel == 1) // standard opaque material
    {
        vec4 diffuseMap = texture(DiffuseTexture, fTexCoord);
        vec4 specularMap = texture(SpecularTexture, fTexCoord);

        vec3 modelNormal;
        if (HasNormalMap != 0)
        {
            vec3 normalMap = normalize(texture(NormalTexture, fTexCoord).rgb);
            vec3 tangent = normalize(fTangent);
            vec3 bitangent = normalize(fBitangent);
            vec3 normal = normalize(fNormal);
            if (dot(cross(normal, tangent), bitangent) < 0)
            {
                tangent = -tangent;
            }
            mat3 tangentModelMatrix = mat3(tangent, bitangent, normal);
            modelNormal = tangentModelMatrix * normalMap;
        }
        else
        {
            modelNormal = normalize(fNormal);
        }

        float a = 30;
        float kA = 0.15;
        float kD = 0.5;
        float kS = 0.7;

        vec3 N = normalize(modelNormal); // normal
        float G = max(0, dot(N, L)); // geometric term
        vec3 R = reflect(-L, N); // reflection direction
        float S = pow(max(0, dot(R, V)), a); // specular coefficient

        vec3 ambient = diffuseMap.rgb * kA;
        vec3 diffuse = G * diffuseMap.rgb * kD * shadowPass;
        vec3 specular = S * specularMap.rgb * kS * shadowPass;

        FragColor = vec4(ambient + diffuse + specular, 1.0);
    }
    else if (IlluminationModel == 2) // Transparent objects
    {
        // note: lots of hacks here specific to hellknight...
        float kA = 0.03;
        float kD = 1.0;
        float kS = 0.1;
        
        vec4 diffuseMap = texture(DiffuseTexture, fTexCoord);
        vec4 specularMap = texture(SpecularTexture, fTexCoord);

        vec3 ambient = diffuseMap.rgb * kA;
        vec3 diffuse = diffuseMap.rgb * kD * shadowPass;
        vec3 specular = specularMap.rgb * kS * shadowPass;

        FragColor = vec4(ambient + diffuse + specular, diffuseMap.a) * fTexCoord.x * fTexCoord.y;
    }

    float kSceneViewRadius = 400.0;
    float kCameraViewRadius = 600.0;
    float sceneViewDistance  = max(length(fPosition) - kSceneViewRadius, 0.0);
    float cameraViewDistance = max(length(fPosition - modelCameraPos) - kCameraViewRadius, 0.0);
    float falloffDistance = max(sceneViewDistance, cameraViewDistance);
    float falloff = clamp(falloffDistance/ kSceneViewRadius, 0.0, 1.0);

    FragColor = mix(FragColor, vec4(BackgroundColor, 1.0), falloff);
}
