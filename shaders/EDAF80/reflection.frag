
#version 410

uniform samplerCube SkyboxTexture;
uniform vec3 camera_position;

in VS_OUT {
    vec3 vertex;
    vec3 normal;
} fs_in;

out vec4 fColor;

void main()
{
    vec3 V = normalize(camera_position);
    vec3 N = normalize(fs_in.normal);
    
    vec3 R = reflect(-V, N);
    
    vec4 reflection = texture(SkyboxTexture, R);
    
    fColor = reflection;
}
