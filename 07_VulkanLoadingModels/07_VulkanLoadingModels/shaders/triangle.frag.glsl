#version 450

layout(location = 0) in vec3 InColor;
layout(location = 1) in vec2 InTexCoord;

layout(binding = 1) uniform sampler2D TexSampler;

layout(location = 0) out vec4 OutColor;

void main()
{
	// OutColor = vec4(InColor, 1.0);
	// OutColor = vec4(InTexCoord, 0.0, 1.0);
	OutColor = texture(TexSampler, InTexCoord);
	// OutColor = texture(TexSampler, InTexCoord * 2.0);
	// OutColor = vec4(InColor * texture(TexSampler, InTexCoord).rgb, 1.0);
}