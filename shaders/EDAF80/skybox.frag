
#version 410

uniform samplerCube sampler;

in VS_OUT {
    vec3 vertex;
} fs_in;

out vec4 fOut;

void main()
{
    fOut = texture(sampler, fs_in.vertex);
}
