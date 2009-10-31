/*
---------------------------------------------------------------------
   Terrain Renderer using texture splatting and geomipmapping
   Copyright (c) 2006 Jelmer Cnossen

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   Jelmer Cnossen
   j.cnossen at gmail dot com
---------------------------------------------------------------------
*/

uniform vec2 invScreenDim; // vec2(1/gu->viewSizeX,1/gu->viewSizeY)

const float shadowBias = 2.1;

// auto-optimized function generated by the GLSL handler
// This function uses the utility functions below to do things like texture reading and lighting
vec4 CalculateColor();

#ifdef DiffuseFromBuffer

	#ifdef UseTextureRECT
		vec4 ReadDiffuseColor()
		{
			return texture2DRect(_buffer, gl_FragCoord.xy);
		}
	#else
		vec4 ReadDiffuseColor()
		{
			return texture2D(_buffer, gl_FragCoord.xy * invScreenDim);
		}
	#endif

#endif


/*  Lighting calculations with bumpmapping */

#ifdef UseBumpMapping
	vec3 reflect(vec3 N, vec3 L)
	{
		return 2.0*N*dot(N, L) - L;
	}

	vec4 Light(vec4 diffuse, vec4 bump)
	{
		vec3 normal = normalize(bump.xyz * vec3(2.0) - vec3(1.0));
		float diffuseFactor = max(0.0, -dot(tsLightDir, normal));
		
		vec3 R = reflect(normal, tsLightDir);
		float specularFactor = clamp(pow(dot(R, tsEyeDir), specularExponent), 0.0, 1.0);
		
	#ifdef UseShadowMapping
		vec4 shadow = shadow2D(shadowMap, shadowTexCoord.xyz, shadowBias);
		diffuseFactor *= shadow.x;
		specularFactor *= shadow.x;
	#endif
	
		vec4 spec = gl_LightSource[0].specular * specularFactor * diffuse.a;
		vec4 r = diffuse * (gl_LightSource[0].diffuse * diffuseFactor + gl_LightSource[0].ambient);
		r += spec;

		return r;
	}
#else

/* Lighting calculations without bumpmapping */

	vec4 Light(vec4 diffuse)
	{	
		float diffuseFactor = max(0.0, -dot(wsLightDir, normal));
		vec3 R = reflect(normal, wsLightDir);
		float specularFactor = clamp(pow(dot(R, wsEyeDir), specularExponent), 0.0, 1.0);

	#ifdef UseShadowMapping
		vec4 shadow = shadow2D(shadowMap, shadowTexCoord.xyz, shadowBias);
		diffuseFactor *= shadow.x;
		specularFactor *= shadow.x;
	#endif
	
		vec4 spec = gl_LightSource[0].specular * specularFactor * diffuse.a;
		vec4 r = diffuse * (gl_LightSource[0].diffuse * diffuseFactor + gl_LightSource[0].ambient);
		r += spec;

		return r;
	}
#endif

// Shader entry point
void main()
{
	gl_FragColor=CalculateColor();
}
