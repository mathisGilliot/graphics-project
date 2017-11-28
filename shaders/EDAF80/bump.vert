#version 410

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;


uniform mat4 vertex_model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform vec3 light_position;
uniform vec3 camera_position;

out VS_OUT {
	vec2 texcoord;
    vec3 vertex;
    vec3 normal;
	vec3 tangent;
	vec3 binormal;
	vec3 fL;
	vec3 fV;
	mat4 WorldIT;
} vs_out;


void main()
{
	vs_out.texcoord = vec2 (texcoord.x,texcoord.y);
	vec3 vert =vec3(vertex_model_to_world * vec4(vertex, 1.0));
	vs_out.vertex = vert;
    vs_out.normal = normalize(normal);
    vs_out.tangent = normalize(tangent);
    vs_out.binormal = normalize(binormal);
	vs_out.fL = normalize(light_position - vert); 
	vs_out.fV = normalize(camera_position - vert);
	vs_out.WorldIT = normal_model_to_world;
    gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);
}




