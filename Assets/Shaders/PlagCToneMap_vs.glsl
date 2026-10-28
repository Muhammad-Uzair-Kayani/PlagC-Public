#version ogre_glsl_ver_330
#ifdef PLAGC_VULKAN_SHADER
#define PC_UNIFORM
#else
#define PC_UNIFORM uniform
#endif
vulkan(layout(ogre_P0) uniform Params {)
PC_UNIFORM mat4 worldViewProj;
vulkan(};)
vulkan_layout(OGRE_POSITION) in vec3 vertex;
vulkan_layout(OGRE_TEXCOORD0) in vec2 uv0;
vulkan_layout(location = 0) out vec2 texCoord;
out gl_PerVertex { vec4 gl_Position; };
void main(){gl_Position=worldViewProj*vec4(vertex,1.0);texCoord=uv0;}
