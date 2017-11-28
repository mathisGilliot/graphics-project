#version 410

uniform samplerCube SkyboxTexture;
uniform sampler2D bump_texture;

uniform vec3 light_position;
uniform vec3 camera_position;

uniform mat4 normal_model_to_world;

in VS_OUT {
    vec3 vertex;
    float dHdX;
    float dHdZ;
    vec2 bumpCoord0;
    vec2 bumpCoord1;
    vec2 bumpCoord2;
} fs_in;

out vec4 frag_color;

void main()
{
    // colors
    vec4 deep = vec4(0.0, 0.0, 0.1, 1.0);
    vec4 shallow = vec4(0.0, 0.5, 0.5, 1.0);

    // calculate normals for bumpmap
    vec3 n_bump = 2.0 * texture(bump_texture, fs_in.bumpCoord0).rgb - 1.0;
    n_bump += 2.0 * texture(bump_texture, fs_in.bumpCoord1).rgb - 1.0;
    n_bump += 2.0 * texture(bump_texture, fs_in.bumpCoord2).rgb - 1.0;
    n_bump = normalize(n_bump);

    mat3 BTN = mat3(vec3(1, fs_in.dHdX, 0),
                    vec3(0, fs_in.dHdZ, 1),
                    vec3(-fs_in.dHdX, 1, -fs_in.dHdZ));
//    vec3 N = normalize(vec3(-fs_in.dHdX, 1, -fs_in.dHdZ));
    vec3 N = normalize(mat3(normal_model_to_world)*BTN*n_bump);

    // light position, camera position, and normal
    vec3 L = normalize(light_position - fs_in.vertex);
    vec3 V = normalize(camera_position - fs_in.vertex);

    // calculating reflection
    vec3 R = reflect(-V, N);
    vec3 reflection = texture(SkyboxTexture, R).rgb;

    // calculate facing color of water
    float facing = 1 - max(dot(V, N), 0);
    vec3 water_color = vec3(mix(deep, shallow, facing));
    
    // fresnel term
    float R0 = 0.02037;
    float fastFresnel = R0 + (1 - R0) * pow((1 - dot(V, N)), 5);
    
    // refraction term
    float ind = 0.75;
    vec3 ref = refract(-V, N, ind);
    vec3 refraction = texture(SkyboxTexture, ref).rgb;

    frag_color = vec4(water_color + reflection * fastFresnel + refraction * (1 - fastFresnel), 1.0f);
}

