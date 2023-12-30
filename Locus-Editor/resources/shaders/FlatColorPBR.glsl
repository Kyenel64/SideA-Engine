// --- FlatColorPBR -----------------------------------------------------------
// PBR shader for non-textured 3D objects.

// --- Vertex Shader ---
#type vertex
#version 450 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec4 a_Albedo;
layout (location = 3) in float a_Metallic;
layout (location = 4) in float a_Roughness;
layout (location = 5) in float a_AO;
layout (location = 6) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec4 u_CameraPosition;
	vec2 u_ViewportSize;
};

layout (location = 0) out vec3 v_FragPos;
layout (location = 1) out vec3 v_Normal;
layout (location = 2) out flat vec4 v_Albedo;
layout (location = 3) out flat float v_Metallic;
layout (location = 4) out flat float v_Roughness;
layout (location = 5) out flat float v_AO;
layout (location = 6) out flat int v_EntityID;
layout (location = 7) out flat vec3 v_ViewPos;

void main()
{
	v_FragPos = a_Position;
	v_Normal = a_Normal;
	v_Albedo = a_Albedo;
	v_Metallic = a_Metallic;
	v_Roughness = a_Roughness;
	v_AO = a_AO;
	v_EntityID = a_EntityID;
	v_ViewPos = u_CameraPosition.xyz;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}



// --- Fragment Shader ---
#type fragment
#version 450 core

struct DirectionalLight
{
	vec4 Direction;
	vec4 Color;
	float Intensity;
	bool Enabled;
	vec2 padding;
};

struct PointLight
{
	vec4 Position;
	vec4 Color;
	float Intensity;
	bool Enabled;
	vec2 padding;
};

struct SpotLight
{
	vec4 Position;
	vec4 Direction;
	vec4 Color;
	float CutOff;
	float OuterCutOff;
	float Intensity;
	bool Enabled;
};

#define MAX_DIRECTIONAL_LIGHTS 16
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 16

const float PI = 3.14159265359;

layout (location = 0) in vec3 v_FragPos;
layout (location = 1) in vec3 v_Normal;
layout (location = 2) in flat vec4 v_Albedo;
layout (location = 3) in flat float v_Metallic;
layout (location = 4) in flat float v_Roughness;
layout (location = 5) in flat float v_AO;
layout (location = 6) in flat int v_EntityID;
layout (location = 7) in flat vec3 v_ViewPos;

layout (std140, binding = 2) uniform Lighting
{
	DirectionalLight u_DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
	PointLight u_PointLights[MAX_POINT_LIGHTS];
	SpotLight u_SpotLights[MAX_SPOT_LIGHTS];
};

layout (location = 0) out vec4 o_Color;
layout (location = 1) out int o_EntityID;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

vec3 CalculatePointLight(PointLight pointLight, vec3 n, vec3 v, vec3 f0);
vec3 CalculateDirectionalLight(DirectionalLight directionalLight, vec3 n, vec3 v, vec3 f0);
vec3 CalculateSpotLight(SpotLight spotLight, vec3 n, vec3 v, vec3 f0);

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 V = normalize(v_ViewPos - v_FragPos);

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, v_Albedo.xyz, v_Metallic);

	vec3 Lo = vec3(0.0);

	// Point lights
	for (int i = 0; i < MAX_POINT_LIGHTS; i++) 
	{
		if (u_PointLights[i].Enabled)
			Lo += CalculatePointLight(u_PointLights[i], N, V, F0);
	}

	// Directional lights
	for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; i++) 
	{
		if (u_DirectionalLights[i].Enabled)
			Lo += CalculateDirectionalLight(u_DirectionalLights[i], N, vec3(1.0f), F0);
	}

	// Spot lights
	for (int i = 0; i < MAX_SPOT_LIGHTS; i++) 
	{
		if (u_SpotLights[i].Enabled)
			Lo += CalculateSpotLight(u_SpotLights[i], N, V, F0);
	}
	
	// Temporary flat ambient color
	vec3 ambient = vec3(0.03) * v_Albedo.xyz * v_AO;
	vec3 color = ambient + Lo;

	// HDR tonemapping
	color = color / (color + vec3(1.0));
	// gamma correct
	color = pow(color, vec3(1.0/2.2)); 

	// --- Outputs ---
	o_Color = vec4(color, 1.0);
	o_EntityID = v_EntityID;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePointLight(PointLight pointLight, vec3 n, vec3 v, vec3 f0)
{
	// calculate per-light radiance
	vec3 L = normalize(pointLight.Position.xyz - v_FragPos);
	vec3 H = normalize(v + L);
	float distance = length(pointLight.Position.xyz - v_FragPos);
	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = pointLight.Color.xyz * pointLight.Intensity * attenuation;

	// Cook-Torrance BRDF
	float NDF = DistributionGGX(n, H, v_Roughness);
	float G   = GeometrySmith(n, v, L, v_Roughness);
	vec3 F    = fresnelSchlick(clamp(dot(H, v), 0.0, 1.0), f0);
	
	vec3 numerator    = NDF * G * F; 
	float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
	vec3 specular = numerator / denominator;
	
	// kS is equal to Fresnel
	vec3 kS = F;
	// for energy conservation, the diffuse and specular light can't
	// be above 1.0 (unless the surface emits light); to preserve this
	// relationship the diffuse component (kD) should equal 1.0 - kS.
	vec3 kD = vec3(1.0) - kS;
	// multiply kD by the inverse metalness such that only non-metals 
	// have diffuse lighting, or a linear blend if partly metal (pure metals
	// have no diffuse light).
	kD *= 1.0 - v_Metallic;

	// scale light by NdotL
	float NdotL = max(dot(n, L), 0.0);

	// add to outgoing radiance Lo
	return (kD * v_Albedo.xyz / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
}

vec3 CalculateDirectionalLight(DirectionalLight directionalLight, vec3 n, vec3 v, vec3 f0)
{
	// calculate per-light radiance
	vec3 L = normalize(-directionalLight.Direction.xyz);
	vec3 H = normalize(v + L);
	vec3 radiance = directionalLight.Color.xyz * directionalLight.Intensity;

	// Cook-Torrance BRDF
	float NDF = DistributionGGX(n, H, v_Roughness);
	float G   = GeometrySmith(n, v, L, v_Roughness);
	vec3 F    = fresnelSchlick(clamp(dot(H, v), 0.0, 1.0), f0);
	
	vec3 numerator    = NDF * G * F; 
	float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
	vec3 specular = numerator / denominator;
	
	// kS is equal to Fresnel
	vec3 kS = F;
	// for energy conservation, the diffuse and specular light can't
	// be above 1.0 (unless the surface emits light); to preserve this
	// relationship the diffuse component (kD) should equal 1.0 - kS.
	vec3 kD = vec3(1.0) - kS;
	// multiply kD by the inverse metalness such that only non-metals 
	// have diffuse lighting, or a linear blend if partly metal (pure metals
	// have no diffuse light).
	kD *= 1.0 - v_Metallic;

	// scale light by NdotL
	float NdotL = max(dot(n, L), 0.0);

	// add to outgoing radiance Lo
	return (kD * v_Albedo.xyz / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
}

vec3 CalculateSpotLight(SpotLight spotLight, vec3 n, vec3 v, vec3 f0)
{
	// calculate per-light radiance
	vec3 L = normalize(spotLight.Position.xyz - v_FragPos);
	vec3 H = normalize(v + L);
	float distance = length(spotLight.Position.xyz - v_FragPos);
	float attenuation = 1.0 / (distance * distance);

	// spotlight calculations
	float theta = dot(L, normalize(-spotLight.Direction.xyz));
	float epsilon = spotLight.CutOff - spotLight.OuterCutOff;
	float intensity = clamp((theta - spotLight.OuterCutOff) / epsilon, 0.0f, 1.0f);
	if (theta < spotLight.OuterCutOff)
		return vec3(0.0f);

	vec3 radiance = spotLight.Color.xyz * spotLight.Intensity * attenuation * intensity;

	// Cook-Torrance BRDF
	float NDF = DistributionGGX(n, H, v_Roughness);
	float G   = GeometrySmith(n, v, L, v_Roughness);
	vec3 F    = fresnelSchlick(clamp(dot(H, v), 0.0, 1.0), f0);
	
	vec3 numerator    = NDF * G * F; 
	float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
	vec3 specular = numerator / denominator;
	
	// kS is equal to Fresnel
	vec3 kS = F;
	// for energy conservation, the diffuse and specular light can't
	// be above 1.0 (unless the surface emits light); to preserve this
	// relationship the diffuse component (kD) should equal 1.0 - kS.
	vec3 kD = vec3(1.0) - kS;
	// multiply kD by the inverse metalness such that only non-metals 
	// have diffuse lighting, or a linear blend if partly metal (pure metals
	// have no diffuse light).
	kD *= 1.0 - v_Metallic;

	// scale light by NdotL
	float NdotL = max(dot(n, L), 0.0);

	// add to outgoing radiance Lo
	return (kD * v_Albedo.xyz / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
}