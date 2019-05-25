#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler2D uColourSampler;
layout(set = 1, binding = 1) uniform sampler2D uPositionSampler;
layout(set = 1, binding = 2) uniform sampler2D uNormalSampler;
layout(set = 1, binding = 3) uniform sampler2D uPBRSampler;
layout(set = 1, binding = 4) uniform sampler2D uPreintegratedFG;
layout(set = 1, binding = 5) uniform samplerCube uEnvironmentMap;
layout(set = 1, binding = 6) uniform sampler2DArray uShadowMap;
layout(set = 1, binding = 7) uniform sampler2D uDepthSampler;

#define MAX_LIGHTS 32
#define MAX_SHADOWMAPS 16

struct Light
{
	vec4 colour;
	vec4 position;
	vec4 direction;
	float intensity;
	float radius;
	float type;
	float p0;
};

layout(std140, binding = 0) uniform UniformBufferLight
{
	Light lights[MAX_LIGHTS];
 	vec4 cameraPosition;
	mat4 viewMatrix;
	mat4 uShadowTransform[MAX_SHADOWMAPS];
    vec4 uSplitDepths[MAX_SHADOWMAPS];
	float lightCount;
	float shadowCount;
	float mode;
	float p1;
} ubo;

#define PI 3.1415926535897932384626433832795
#define GAMMA 2.2

struct Material
{
	vec4 albedo;
	vec3 specular;
	float roughness;
	vec3 normal;
	float ao;
};

vec3 FinalGamma(vec3 color)
{
	return pow(color, vec3(1.0 / GAMMA));
}

vec3 GammaCorrectTextureRGB(vec3 texCol)
{
	return vec3(pow(texCol.rgb, vec3(GAMMA)));
}

float FresnelSchlick(float f0, float fd90, float view)
{
	return f0 + (fd90 - f0) * pow(max(1.0 - view, 0.1), 5.0);
}

float Disney(Light light, Material material, vec3 eye)
{
	vec3 halfVector = normalize(light.direction.xyz + eye);

	float NdotL = max(dot(material.normal, light.direction.xyz), 0.0);
	float LdotH = max(dot(light.direction.xyz, halfVector), 0.0);
	float NdotV = max(dot(material.normal, eye), 0.0);

	float energyBias = mix(0.0, 0.5, material.roughness);
	float energyFactor = mix(1.0, 1.0 / 1.51, material.roughness);
	float fd90 = energyBias + 2.0 * (LdotH * LdotH) * material.roughness;
	float f0 = 1.0;

	float lightScatter = FresnelSchlick(f0, fd90, NdotL);
	float viewScatter = FresnelSchlick(f0, fd90, NdotV);

	return lightScatter * viewScatter * energyFactor;
}

vec3 GGX(Light light, Material material, vec3 eye)
{
	vec3 h = normalize(light.direction.xyz + eye);
	float NdotH = max(dot(material.normal, h), 0.0);

	float rough2 = max(material.roughness * material.roughness, 2.0e-3); // capped so spec highlights doesn't disappear
	float rough4 = rough2 * rough2;

	float d = (NdotH * rough4 - NdotH) * NdotH + 1.0;
	float D = rough4 / (PI * (d * d));

	// Fresnel
	vec3 reflectivity = material.specular;
	float fresnel = 1.0;
	float NdotL = clamp(dot(material.normal, light.direction.xyz), 0.0, 1.0);
	float LdotH = clamp(dot(light.direction.xyz, h), 0.0, 1.0);
	float NdotV = clamp(dot(material.normal, eye), 0.0, 1.0);
	vec3 F = reflectivity + (fresnel - fresnel * reflectivity) * exp2((-5.55473 * LdotH - 6.98316) * LdotH);

	// geometric / visibility
	float k = rough2 * 0.5;
	float G_SmithL = NdotL * (1.0 - k) + k;
	float G_SmithV = NdotV * (1.0 - k) + k;
	float G = 0.25 / (G_SmithL * G_SmithV);

	return G * D * F;
}

vec3 RadianceIBLIntegration(float NdotV, float roughness, vec3 specular)
{
	vec2 preintegratedFG = texture(uPreintegratedFG, vec2(roughness, 1.0 - NdotV)).rg;
	return specular * preintegratedFG.r + preintegratedFG.g;
}

vec3 IBL(Light light, Material material, vec3 eye)
{
	float NdotV = max(dot(material.normal, eye), 0.0);

	vec3 reflectionVector = normalize(reflect(-eye, material.normal));
	float smoothness = 1.0 - material.roughness;
	float mipLevel = (1.0 - smoothness * smoothness) * 10.0;
	vec4 cs = textureLod(uEnvironmentMap, reflectionVector, mipLevel);
	vec3 result = pow(cs.xyz, vec3(GAMMA)) * RadianceIBLIntegration(NdotV, material.roughness, material.specular);

	vec3 diffuseDominantDirection = material.normal;
	float diffuseLowMip = 9.6;
	vec3 diffuseImageLighting = textureLod(uEnvironmentMap, diffuseDominantDirection, diffuseLowMip).rgb;
	diffuseImageLighting = pow(diffuseImageLighting, vec3(GAMMA));

	return result + diffuseImageLighting * material.albedo.rgb;
}

float Diffuse(Light light, Material material, vec3 eye)
{
	return Disney(light, material, eye);
}

vec3 Specular(Light light, Material material, vec3 eye)
{
	return GGX(light, material, eye);
}

float Attentuate( vec3 lightData, float dist )
{
	float att =  1.0 / ( lightData.x + lightData.y*dist + lightData.z*dist*dist );
	float damping = 1.0;// - (dist/lightData.w);
	return max(att * damping, 0.0);
}

float DoShadowTest(vec3 tsShadow, int tsLayer, vec2 pix)
{
	vec4 tCoord;
	tCoord.xyw = tsShadow;
	tCoord.z = float(tsLayer);

	if (tsLayer > 0)
	{
		return 0.0f;//texture(uShadowMap, tCoord);
	}
	else
	{
		float shadow = 0.0f;
		for (float y = -1.5f; y <= 1.5f; y += 1.0f)
			for (float x = -1.5f; x <= 1.5f; x += 1.0f)
				//shadow += texture(uShadowMap, tCoord + vec4(pix.x * x, pix.y * y, 0, 0));

		return shadow / 16.0f;
	}
}

const mat4 biasMat = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0
);

#define ambient 0.3

float textureProj(vec4 P, vec2 offset, int cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.001f; //0.005

	vec4 shadowCoord = P / P.w;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
	{
		float dist = texture(uShadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).x;//, shadowCoord.z));
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias)
		{
			shadow = ambient;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, int cascadeIndex)
{
	ivec2 texDim = textureSize(uShadowMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

layout(location = 0) out vec4 outColor;

const float NORMAL_BIAS = 0.002f;

void main()
{
	vec4 colourTex   = texture(uColourSampler   , fragTexCoord);

    if(colourTex.w < 0.1)
        discard;

    vec3 positionTex = texture(uPositionSampler , fragTexCoord).rgb;
    vec4 pbrTex		 = texture(uPBRSampler   , fragTexCoord);
    vec3 normal      = normalize(texture(uNormalSampler, fragTexCoord).rgb);

    vec3  spec      = vec3(pbrTex.x);
	float roughness = pbrTex.y;

    vec3 wsPos      = positionTex;

    vec3 finalColour;

    Material material;
    material.albedo    = colourTex;
    material.specular  = spec;
    material.roughness = roughness;
    material.normal    = normal;
	material.ao		   = pbrTex.z;

    vec3 eye      = normalize(ubo.cameraPosition.xyz - wsPos);
    vec4 diffuse  = vec4(0.0);
    vec3 specular = vec3(0.0);

	int cascadeIndex = 0;
	vec4 viewPos = ubo.viewMatrix * vec4(wsPos, 1.0);

	for(int i = 0; i < ubo.shadowCount - 1; ++i)
	{
		if(viewPos.z < ubo.uSplitDepths[i].x)
		{
			cascadeIndex = i + 1;
		}
	}
	
	vec4 shadowCoord = (biasMat * ubo.uShadowTransform[cascadeIndex]) * vec4(wsPos, 1.0);

	float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
	//float shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0f), cascadeIndex);

	for(int i = 0; i < ubo.lightCount; ++i)
	{
		Light light = ubo.lights[i];

		float value = shadow;
		if(light.type > 0.0)
		{
		    // Vector to light
			vec3 L = light.position.xyz - wsPos;
			// Distance from light to fragment position
			float dist = length(L);
		
			// Light to fragment
			L = normalize(L);

			// Attenuation
			float atten = light.radius / (pow(dist, 2.0) + 1.0);
			
			value = atten;

			light.direction = vec4(L,1.0);
		}

		float NdotL = clamp(dot(material.normal, light.direction.xyz), 0.0, 1.0)  * value;
		diffuse  += NdotL * Diffuse(light, material, eye) * light.colour * light.intensity;
		specular += NdotL * Specular(light, material, eye) * light.colour.xyz * light.intensity;
	}
  

    diffuse = max(diffuse, vec4(0.1));

    finalColour = (material.albedo.xyz * diffuse.rgb + specular) * material.ao;
    //finalColour = material.albedo.xyz * diffuse.rgb + (specular + IBL(light, material, eye));

	finalColour = FinalGamma(finalColour);
	outColor = vec4(finalColour, 1.0);

	if(ubo.mode > 0.5)
	{
		if(ubo.mode == 1.0)
		{
			outColor = colourTex;
		}

		else if(ubo.mode == 2.0)
		{
			outColor = vec4(pbrTex.x);
		}

		else if(ubo.mode == 3.0)
		{
			outColor = vec4(pbrTex.y);
		}

		else if(ubo.mode == 4.0)
		{
			outColor = vec4(pbrTex.z);
		}
	
		else if(ubo.mode == 5.0)
		{
			outColor = vec4(normal,1.0);
		}
	}
}
