#version 450 core
// Classic Phong Shader (Vertex Normals Only)
// Copyright (c) 2026 Antigravity

const int NumLights = 3;

struct AnalyticalLight {
	vec3 direction;
	vec3 radiance;
};

layout(location=0) in Vertex
{
	vec3 position;
	vec2 texcoord;
	mat3 tangentBasis;
} vin;

layout(location=0) out vec4 color;

layout(std140, binding=1) uniform ShadingUniforms
{
	AnalyticalLight lights[NumLights];
	vec3 eyePosition;
	vec4 flags;
};

layout(binding=0) uniform sampler2D albedoTexture;
layout(binding=2) uniform sampler2D metalnessTexture;
layout(binding=3) uniform sampler2D roughnessTexture;

void main()
{
	// Classic Phong uses Vertex Normals (ignore normal map)
	vec3 N = normalize(vin.tangentBasis[2]);
	
	// Sample textures with toggle support
	vec3 albedo = flags.x > 0.5 ? texture(albedoTexture, vin.texcoord).rgb : vec3(0.5);
	float metalness = flags.z > 0.5 ? texture(metalnessTexture, vin.texcoord).r : 0.0;
	float roughness = flags.w > 0.5 ? texture(roughnessTexture, vin.texcoord).r : 0.5;
	
	vec3 V = normalize(eyePosition - vin.position);
	
	// --- Material Interpretation for Phong ---
	vec3 diffuseColor = albedo * (1.0 - metalness);
	vec3 specularColor = mix(vec3(1.0), albedo, metalness);
	float shininess = mix(128.0, 4.0, roughness);

	vec3 totalLighting = vec3(0.0);
	
	// --- Brighter Constant Ambient ---
	vec3 ambient = 0.2 * albedo;
	totalLighting += ambient;

	// --- Virtual Overhead Light ---
	{
		vec3 overheadDir = normalize(vec3(0.0, 1.0, 0.2));
		vec3 overheadRadiance = vec3(0.7);
		float ohDiff = max(dot(N, overheadDir), 0.0);
		totalLighting += ohDiff * diffuseColor * overheadRadiance;
		
		vec3 ohR = reflect(-overheadDir, N);
		float ohSpec = pow(max(dot(ohR, V), 0.0), shininess);
		totalLighting += ohSpec * specularColor * overheadRadiance * 0.5;
	}

	// --- Analytical Lights ---
	for(int i=0; i<NumLights; ++i)
	{
		vec3 Li = normalize(-lights[i].direction.xyz);
		vec3 Lradiance = lights[i].radiance.xyz;
		
		// Diffuse (Lambert)
		float cosTheta = max(dot(N, Li), 0.0);
		vec3 diffuse = cosTheta * diffuseColor * Lradiance;
		
		// Specular (Classic Phong)
		vec3 R = reflect(-Li, N);
		float cosAlpha = max(dot(R, V), 0.0);
		vec3 specular = pow(cosAlpha, shininess) * specularColor * Lradiance;
		
		totalLighting += diffuse + specular;
	}
	
	color = vec4(totalLighting, 1.0);
}
