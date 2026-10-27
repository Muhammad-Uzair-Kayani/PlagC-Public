#version 330 core
uniform samplerCube skyCubemap;
in vec3 cameraDirection;
out vec4 fragColour;
void main()
{
    vec3 worldDir = normalize(cameraDirection);
    // Ogre cubemap sampling uses left-handed texture coordinates.
    vec3 env = texture(skyCubemap, vec3(worldDir.xy, -worldDir.z)).rgb;

    // Match OUTPOST ZERO's late-afternoon directional key.  The cubemap remains the reflection
    // source; this atmospheric layer only enriches the visible sky and therefore does not fake IBL.
    vec3 sunDir = normalize(vec3(0.6, 1.0, 0.4));
    float horizon = pow(1.0 - clamp(abs(worldDir.y), 0.0, 1.0), 4.0);
    vec3 horizonTint = vec3(0.95, 0.43, 0.18) * horizon * 0.24;
    float sun = pow(max(dot(worldDir, sunDir), 0.0), 850.0);
    float corona = pow(max(dot(worldDir, sunDir), 0.0), 32.0);
    vec3 sunColour = vec3(1.0, 0.72, 0.38) * (sun * 7.0 + corona * 0.16);

    // Slight zenith lift preserves colour separation after the HDR filmic pass.
    float zenith = smoothstep(0.0, 0.8, worldDir.y);
    vec3 atmosphere=mix(vec3(.42,.48,.52),vec3(.08,.23,.43),zenith);
    vec3 sky = mix(env * mix(vec3(1.0), vec3(0.88, 0.94, 1.08), zenith),atmosphere,.93);
    fragColour = vec4(sky + horizonTint + sunColour, 1.0);
}
