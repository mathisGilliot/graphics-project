

#version 410

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;

uniform mat4 vertex_model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform float time;
uniform float wave1Params[6];
uniform float wave2Params[6];

out VS_OUT {
    vec3 vertex;
    float dHdX;
    float dHdZ;
    vec2 bumpCoord0;
    vec2 bumpCoord1;
    vec2 bumpCoord2;
} vs_out;


void main()
{
    // values for calculating wave shapes: Amplitude, Frequency, Phase, Sharpness, Direction
    float yA = wave1Params[0], yF = wave1Params[1], yP = wave1Params[2], yS = wave1Params[3], yDx = wave1Params[4], yDz = wave1Params[5];
    float y2A = wave2Params[0], y2F = wave2Params[1], y2P = wave2Params[2], y2S = wave2Params[3], y2Dx = wave2Params[4], y2Dz = wave2Params[5];
    
    // calculating two waves to displace vertices
    float y = yA * pow(sin((yDx * vertex.x + yDz * vertex.z) * yF + time * yP) * 0.5 + 0.5, yS);
    float y2 = y2A * pow(sin((y2Dx * vertex.x + y2Dz * vertex.z) * y2F + time * y2P) * 0.5 + 0.5, y2S);

    // calculating partial derivatives for x and z
    vs_out.dHdX =
    0.5 * yS * yF * yA * pow((sin((yDx * vertex.x + yDz * vertex.z) * yF + time * yP) * 0.5 + 0.5), yS-1) * (cos((yDx * vertex.x + yDz * vertex.z) * yF + time * yP) * yDx)
    + 0.5 * y2S * y2F * y2A * pow((sin((y2Dx * vertex.x + y2Dz * vertex.z) * y2F + time * y2P) * 0.5 + 0.5), y2S-1) * (cos((y2Dx * vertex.x + y2Dz * vertex.z) * y2F + time * y2P) * y2Dx);

    vs_out.dHdZ =
    0.5 * yS * yF * yA * pow((sin((yDx * vertex.x + yDz * vertex.z) * yF + time * yP) * 0.5 + 0.5), yS-1) * (cos((yDx * vertex.x + yDz * vertex.z) * yF + time * yP) * yDz)
    + 0.5 * y2S * y2F * y2A * pow((sin((y2Dx * vertex.x + y2Dz * vertex.z) * y2F + time * y2P) * 0.5 + 0.5), y2S-1) * (cos((y2Dx * vertex.x + y2Dz * vertex.z) * y2F + time * y2P) * y2Dz);

    // calculating coordinate pairs for bump mapping
    vec2 texScale = vec2(8.0, 4.0f);
    float bumpTime = mod(time, 100.0f);
    vec2 bumpSpeed = vec2(-0.01, 0.0f);

    vs_out.bumpCoord0.xy = texcoord.xy * texScale + bumpTime * bumpSpeed;
    vs_out.bumpCoord1.xy = texcoord.xy * texScale * 2 + bumpTime * bumpSpeed * 4;
    vs_out.bumpCoord2.xy = texcoord.xy * texScale * 4 + bumpTime * bumpSpeed * 8;

    // setting out values
    vs_out.vertex = vec3(vertex_model_to_world * vec4(vertex.x, y + y2, vertex.z, 1.0));

    // setting position of vertex
    gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(vertex.x, y + y2, vertex.z, 1.0);
}


