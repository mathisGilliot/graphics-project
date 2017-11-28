#version 410


uniform samplerCube my_cube_map;
in VS_OUT {
    vec3 vertex;
} fs_in;

out vec4 cubemap_color;

void main()
{
   
	cubemap_color = texture(my_cube_map, fs_in.vertex);
}
