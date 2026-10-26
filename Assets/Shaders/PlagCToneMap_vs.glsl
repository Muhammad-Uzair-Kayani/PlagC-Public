#version 330 core
in vec3 vertex;
in vec2 uv0;
uniform mat4 worldViewProj;
out vec2 texCoord;
void main(){gl_Position=worldViewProj*vec4(vertex,1.0);texCoord=uv0;}
