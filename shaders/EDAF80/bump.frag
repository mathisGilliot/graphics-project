#version 410

uniform vec3 light_position;
uniform vec3 camera_position;
uniform sampler2D bumptex;
uniform sampler2D diffusetex;
uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;
in VS_OUT {
    vec2 texcoord;
    vec3 vertex;
    vec3 normal;
	vec3 tangent;
	vec3 binormal;
	vec3 fL;
	vec3 fV;
	mat4 WorldIT;
} fs_in;

out vec4 bump_color;

void main()
{
	vec3 bump_normal= 2.0*(texture(bumptex,fs_in.texcoord).rgb)-1.0;
    bump_normal = normalize(bump_normal);
	mat3 TBN = mat3(fs_in.tangent,fs_in.binormal,fs_in.normal);
    vec3 L = normalize(fs_in.fL);
    vec3 N = normalize(mat3(fs_in.WorldIT)*TBN*bump_normal);
	vec3 V = normalize(camera_position);
    
    vec3 R = normalize(reflect(-L, N));
    
    vec3 kd = texture(diffusetex,fs_in.texcoord).rgb * max(dot(N, L), 0.0);
	vec3 ks = specular * pow(max(dot(R, V), 0), shininess);
    
    bump_color = vec4(kd + ks, 1);
}
