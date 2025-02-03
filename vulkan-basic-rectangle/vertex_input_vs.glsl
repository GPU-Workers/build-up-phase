#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPosition;

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
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);

    fragPosition = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragColor = inColor;

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
}