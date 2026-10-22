#version 330 core
uniform samplerCube skyCubemap;
in vec3 cameraDirection;
out vec4 fragColour;
void main()
{
    // Ogre cubemap sampling uses left-handed texture coordinates.
    fragColour = texture(skyCubemap, vec3(cameraDirection.xy, -cameraDirection.z));
}
