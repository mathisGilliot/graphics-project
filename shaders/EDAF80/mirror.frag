
#version 410

uniform vec3 light_position;
uniform vec3 camera_position;


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
    
    // calculating reflection
    vec3 R = reflect(-V, N);
    // vec3 reflection = texture(SkyboxTexture, R).rgb;
    
    vec3 kd = vec3(0,0,0) * max(dot(N, L), 0);
    vec3 ks = vec3(0.5,0.5,0.5) * pow(max(dot(R, V), 0), 125);
    
    phong_color = vec4(vec3(0,0,0) + kd + ks, 1);
}
