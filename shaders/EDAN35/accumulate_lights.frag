#version 410

in vec4 gl_FragCoord;

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2DShadow shadow_texture;

uniform vec2 inv_res;

uniform mat4 view_projection_inverse;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 light_color;
uniform vec3 light_position;
uniform vec3 light_direction;
uniform float light_intensity;
uniform float light_angle_falloff;

uniform vec2 shadowmap_texel_size;


layout (location = 0) out vec4 light_diffuse_contribution;
layout (location = 1) out vec4 light_specular_contribution;


void main()
{
	/* 1. Calculate texture coordinates from screenposition */
	vec2 screenUV = vec2(gl_FragCoord.x*inv_res.x, gl_FragCoord.y*inv_res.y); // in [0,1]
	
	/* 2. Extract normal from geometry buffer */
	vec3 nN = normalize(texture(normal_texture, screenUV)*2.0-1.0).xyz;
	
	/* 3. Extract depth from geometry buffer */
	// Get the depth buffer value at this pixel.
	float depth = texture(depth_texture, screenUV).x;
	
	/* 4. Perform inverse projection to obtain world space position */
	// screenPos is the viewport position at this pixel in the range -1 to 1.
	vec4 screenPos = vec4(screenUV.x * 2.0 - 1.0,
						  screenUV.y * 2.0 - 1.0,
						  depth * 2.0 - 1.0, 1.0);
	
	// Transform by the view-projection inverse.
	vec4 currentPos = view_projection_inverse * screenPos;
	
	// World space
	vec3 worldPos = currentPos.xyz / currentPos.w;
	
	/* 5. Phong model */
	vec3 nV = normalize(camera_position - worldPos);
	vec3 L = light_position - worldPos;
	vec3 nL = normalize(L);
	vec3 nR = normalize(reflect(-nL,nN));
	
	float sqDist = dot(L, L);
	float dist_falloff = 1.0/sqDist;
	float angle = acos(dot(-nL,light_direction));
	float angular_falloff = max(0.0, 1.0-angle/light_angle_falloff);
	float falloff = dist_falloff * angular_falloff;
	
	float visibility = 0.0;
	vec4 texel = shadow_view_projection * vec4(worldPos, 1.0);
	vec4 realTexel = vec4(texel.xyz /texel.w, 1.0);
	
	/* Sampling */
	// 4x4 sampling to make aliasing smoother
	for (int i = -2; i < 2; i++){
		for (int j = -2; j < 2; j++){
			visibility += texture(shadow_texture, realTexel.xyz*0.5+0.5 + vec3(shadowmap_texel_size.x*i, shadowmap_texel_size.y*j, 0.0));
		}
	}
	visibility /= 16;
	
	// Without sampling : Simple depth test
	//if (depth <= texture(shadow_texture, realTexel.xyz*0.5+0.5)) {
	//	visibility = 1.0;
	//}
	light_diffuse_contribution = vec4(visibility*falloff*light_color*light_intensity*max(dot(nL, nN), 0.0), 1.0);
	light_specular_contribution = vec4(visibility*falloff*light_color*light_intensity*pow(max(dot(nV,nR),0.0), 10), 1.0);
	
	//light_diffuse_contribution  = vec4(0.2, 0.0, 0.0, 1.0);
	//light_specular_contribution = vec4(0.0, 0.0, 0.0, 1.0);
}

