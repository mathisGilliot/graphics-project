#version 410

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
	vec2 texCoord = gl_FragCoord.xy*inv_res;
	float depth = texture(depth_texture, texCoord).x;
	vec4 NSS = vec4(texCoord,depth,1);
	vec4 NDC = NSS*2-1;
	vec4 vertex = view_projection_inverse*NDC;
	vertex=vertex/vertex.w;

	vec3 normal = texture(normal_texture, texCoord).xyz*2-1;

	vec3 L = normalize(light_position - vertex.xyz);
    vec3 N = normalize(normal);
    vec3 V = normalize(camera_position - vertex.xyz);
    vec3 R = normalize(reflect(-L, N));

	float dist_falloff= 1/dot((-light_position+vertex.xyz),(-light_position+vertex.xyz));
	float angle= acos(dot(normalize(-light_position+vertex.xyz),light_direction));
	float ang_falloff= max((light_angle_falloff-angle)/light_angle_falloff,0);

	vec4 shadowpos= shadow_view_projection*vertex;
	shadowpos=shadowpos/shadowpos.w;
	vec3 shadowCoord= (shadowpos.xyz+1)/2;

	float off=0;
	for (int i=-10;i<=10;i+=5){
		for (int j=-10;j<=10;j+=5){
			 off+= texture(shadow_texture,shadowCoord+vec3(shadowmap_texel_size.x*i,shadowmap_texel_size.y*j,0),0.00005);
		}
	}
	off/=25;

	light_diffuse_contribution  = vec4(off*light_intensity*dist_falloff*ang_falloff*light_color*max(dot(N, L), 0), 1.0);
	light_specular_contribution = vec4(off*light_intensity*dist_falloff*ang_falloff*light_color*pow(max(dot(R, V), 0), 5), 1.0);
}
