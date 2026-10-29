#version ogre_glsl_ver_330
vulkan_layout(ogre_t0) uniform textureCube skyCubemap;
vulkan(layout(ogre_s0) uniform sampler skyCubemapSampler);
vulkan_layout(location = 0) in vec3 cameraDirection;
vulkan_layout(location = 0) out vec4 fragColour;
void main()
{
    vec3 worldDir = normalize(cameraDirection);
    // Ogre cubemap sampling uses left-handed texture coordinates.
    vec3 env = texture(vkSamplerCube(skyCubemap, skyCubemapSampler), vec3(worldDir.xy, -worldDir.z)).rgb;

    // Pre-normalized constant avoids a per-fragment normalize. Integer multiplication chains
    // preserve the original powers while avoiding generic pow() lowering on Vulkan drivers.
    const vec3 sunDir = vec3(0.4866642634, 0.8111071057, 0.3244428423);
    float horizonBase = 1.0 - clamp(abs(worldDir.y), 0.0, 1.0);
    float horizon2 = horizonBase * horizonBase;
    float horizon = horizon2 * horizon2;
    vec3 horizonTint = vec3(0.95, 0.43, 0.18) * horizon * 0.24;

    float sunDot = max(dot(worldDir, sunDir), 0.0);
    float sun2 = sunDot * sunDot;
    float sun4 = sun2 * sun2;
    float sun8 = sun4 * sun4;
    float sun16 = sun8 * sun8;
    float sun32 = sun16 * sun16;
    float sun64 = sun32 * sun32;
    float sun128 = sun64 * sun64;
    float sun256 = sun128 * sun128;
    float sun512 = sun256 * sun256;
    float sun = sun512 * sun256 * sun64 * sun16 * sun2; // x^850
    float corona = sun32;                                // x^32
    vec3 sunColour = vec3(1.0, 0.72, 0.38) * (sun * 7.0 + corona * 0.16);

    // Slight zenith lift preserves colour separation after the HDR filmic pass.
    float zenith = smoothstep(0.0, 0.8, worldDir.y);
    vec3 atmosphere = mix(vec3(.42, .48, .52), vec3(.08, .23, .43), zenith);
    vec3 sky = mix(env * mix(vec3(1.0), vec3(0.88, 0.94, 1.08), zenith), atmosphere, .93);
    fragColour = vec4(sky + horizonTint + sunColour, 1.0);
}
