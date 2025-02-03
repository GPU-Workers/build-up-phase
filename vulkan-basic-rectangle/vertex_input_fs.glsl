#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    
    vec3 lightPos;
    float padding1;

    vec3 lightColor;
    float padding2;
    
    vec3 cameraPos;
    float padding3;
} ubo;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(ubo.lightPos - fragPosition);

    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * ubo.lightColor;

    // diffusion
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * ubo.lightColor;

    // specular
    vec3 viewDir = normalize(ubo.cameraPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    float specularStrength = 0.5;
    vec3 specular = specularStrength * spec * ubo.lightColor;

    outColor = vec4((ambient + diffuse + specular) * fragColor, 1.0);
}
