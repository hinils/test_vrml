#version 330 core

in vec3 vFragPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;
uniform vec3 uDiffuse;
uniform vec3 uSpecular;
uniform vec3 uEmissive;
uniform float uShininess;
uniform float uAmbientIntensity;
uniform int   uUseVertexColor;

out vec4 FragColor;

void main() {
    vec3 baseColor = (uUseVertexColor == 1) ? vColor : uDiffuse;

    // Ambient
    vec3 ambient = uAmbientIntensity * uLightColor * baseColor;

    // Diffuse
    vec3 norm     = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * uLightColor * baseColor;

    // Specular (Blinn-Phong)
    vec3 viewDir  = normalize(uViewPos - vFragPos);
    vec3 halfVec  = normalize(lightDir + viewDir);
    float spec    = pow(max(dot(norm, halfVec), 0.0), max(uShininess * 128.0, 1.0));
    vec3 specular = spec * uLightColor * uSpecular;

    // Fill light from opposite direction
    vec3 fillDir  = -lightDir;
    float fillDiff= max(dot(norm, fillDir), 0.0) * 0.3;
    vec3 fill     = fillDiff * vec3(0.4, 0.5, 0.7) * baseColor;

    vec3 result = ambient + diffuse + specular + fill + uEmissive;
    FragColor   = vec4(result, 1.0);
}
