#version 450

layout(location = 0) in vec3 VertPosition;
layout(location = 1) in vec3 VertColor;
layout(location = 2) in vec2 VertTexCoord;

layout(location = 0) out vec3 FragColor;
layout(location = 1) out vec2 FragTexCoord;

layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

void main()
{
	mat4 MVP = ubo.proj * ubo.view * ubo.model;
    gl_Position = MVP * vec4(VertPosition, 1.0);

	FragColor = VertColor;
	FragTexCoord = VertTexCoord;
}