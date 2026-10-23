#version 330 core
in vec3 vertex;
in vec3 normal;
uniform mat4 worldViewProj;
out vec3 cameraDirection;
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
