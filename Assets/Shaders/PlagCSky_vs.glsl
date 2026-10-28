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
vulkan_layout(OGRE_NORMAL) in vec3 normal;
vulkan_layout(location = 0) out vec3 cameraDirection;
out gl_PerVertex { vec4 gl_Position; };
void main()
{
    vec4 clip = worldViewProj * vec4(vertex, 1.0);
#ifdef OGRE_NO_REVERSE_DEPTH
    gl_Position = clip.xyww;
#else
    gl_Position = vec4(clip.xy, 0.0, clip.w);
#endif
    // Compositor CAMERA_DIRECTION gives rotation-aware world-space rays.
    cameraDirection = normal;
}
