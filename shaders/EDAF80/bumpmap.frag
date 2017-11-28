

#version 410

uniform sampler2D bump_texture;
uniform sampler2D bump_diffuse;

uniform mat4 normal_model_to_world;

uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

uniform vec3 light_position;
uniform vec3 camera_position;

in VS_OUT {
    vec3 vertex;
    vec3 normal;
    vec2 texcoord;
    vec3 tangent;
    vec3 binormal;
    vec3 light_position;
    vec3 camera_position;
} fs_in;

out vec4 bump_color;

void main()
{
    // matrix to transform from tangent into world space
    mat3 TBN = mat3(fs_in.tangent, fs_in.binormal, fs_in.normal);
    
    vec3 light_position = normalize(fs_in.light_position);
    vec3 camera_position = normalize(fs_in.camera_position);
    
    // remapping from [0, 1] to [-1, 1]
    vec3 bump_normal = normalize(2.0 * texture(bump_texture, fs_in.texcoord).rgb - 1.0);
    
    // find light position, normal position (in world space, relative to surace of sphere), and camera position
    vec3 L = normalize(fs_in.light_position);
    vec3 N = normalize(mat3(normal_model_to_world) * TBN * bump_normal);
    vec3 V = normalize(camera_position);
    
    // calculate reflection angle for shininess
    vec3 R = normalize(reflect(-L, N));
    
    // get diffuse color by looking up rgb value from texture
    // calculate specular like phong shader
    vec3 kd = texture(bump_diffuse, fs_in.texcoord).rgb * max(dot(N, L), 0);
    vec3 ks = specular * pow(max(dot(R, V), 0), shininess);
    
    if(shininess>0)
    {
        bump_color = vec4(ambient + kd + ks, 1);
    }
    else
    {
        bump_color = vec4(ambient + kd, 1);
    }
}
