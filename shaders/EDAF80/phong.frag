
#version 410

uniform vec3 light_position;
uniform vec3 camera_position;

uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

in VS_OUT {
    vec3 vertex;
    vec3 normal;
} fs_in;

out vec4 phong_color;

void main()
{
    vec3 L = normalize(light_position - fs_in.vertex);
    vec3 N = normalize(fs_in.normal);
    vec3 V = normalize(camera_position - fs_in.vertex);
    
    vec3 R = normalize(reflect(-L, N));
    
    vec3 kd = diffuse * max(dot(N, L), 0);
    vec3 ks = specular * pow(max(dot(R, V), 0), shininess);
    
    phong_color = vec4(ambient + kd + ks, 1);
}
