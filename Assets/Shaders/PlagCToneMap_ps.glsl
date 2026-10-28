#version ogre_glsl_ver_330
#ifdef PLAGC_VULKAN_SHADER
#define PC_UNIFORM
#else
#define PC_UNIFORM uniform
#endif
vulkan(layout(ogre_P0) uniform Params {)
PC_UNIFORM float exposure;
PC_UNIFORM float bloomStrength;
PC_UNIFORM float cameraNear, cameraFar, fogDensity;
PC_UNIFORM vec3 fogColour;
vulkan(};)
vulkan_layout(ogre_t0) uniform texture2D sceneTexture;
vulkan(layout(ogre_s0) uniform sampler sceneTextureSampler);
vulkan_layout(ogre_t1) uniform texture2D bloomTexture;
vulkan(layout(ogre_s1) uniform sampler bloomTextureSampler);
vulkan_layout(ogre_t2) uniform texture2D depthTexture;
vulkan(layout(ogre_s2) uniform sampler depthTextureSampler);
vulkan_layout(location = 0) in vec2 texCoord;
vulkan_layout(location = 0) out vec4 fragColour;

float luma(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 sampleScene(vec2 uv)
{
    return max(texture(vkSampler2D(sceneTexture, sceneTextureSampler), clamp(uv, vec2(0.0), vec2(1.0))).rgb, vec3(0.0));
}

// Lightweight FXAA performed before tone mapping.  Keeping it in the final HDR pass avoids
// another full-resolution render target while still cleaning the high-contrast facility edges.
vec3 fxaa(vec2 uv)
{
    vec2 px = 1.0 / vec2(textureSize(vkSampler2D(sceneTexture, sceneTextureSampler), 0));
    vec3 c  = sampleScene(uv);
    vec3 n  = sampleScene(uv + vec2(0.0, -px.y));
    vec3 s  = sampleScene(uv + vec2(0.0,  px.y));
    vec3 e  = sampleScene(uv + vec2( px.x, 0.0));
    vec3 w  = sampleScene(uv + vec2(-px.x, 0.0));
    float lc = luma(c), ln = luma(n), ls = luma(s), le = luma(e), lw = luma(w);
    float lo = min(lc, min(min(ln, ls), min(le, lw)));
    float hi = max(lc, max(max(ln, ls), max(le, lw)));
    float range = hi - lo;
    if (range < max(0.0312, hi * 0.125))
        return c;
    vec2 dir = vec2(-(ln - ls), le - lw);
    float inv = 1.0 / max(abs(dir.x) + abs(dir.y), 1e-5);
    dir = clamp(dir * inv, vec2(-1.0), vec2(1.0)) * px * 1.35;
    return (sampleScene(uv - dir * 0.5) + sampleScene(uv + dir * 0.5) + c * 2.0) * 0.25;
}

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    vec3 hdr = fxaa(texCoord);
    float depth=texture(vkSampler2D(depthTexture, depthTextureSampler),texCoord).r;
    // Ogre GL3+ uses reversed Z. Reconstruct axial view distance from the depth buffer.
    float distanceToCamera=cameraNear*cameraFar/max(cameraNear+depth*(cameraFar-cameraNear),.00001);
    float fog=depth>.000001 ? 1.0-exp(-max(0.0,distanceToCamera-25.0)*fogDensity) : 0.0;
    hdr=mix(hdr,fogColour,clamp(fog,0.0,.93));
    hdr += max(texture(vkSampler2D(bloomTexture, bloomTextureSampler), texCoord).rgb, vec3(0)) * bloomStrength;
    hdr *= exposure;

    // Very small cinematic grade: preserve neutral materials but push highlights slightly warm
    // and shadows slightly cool.  This is deliberately subtle so the scene remains a renderer test.
    float y = luma(hdr);
    vec3 shadowTint = vec3(0.985, 1.0, 1.025);
    vec3 highlightTint = vec3(1.025, 1.005, 0.975);
    hdr *= mix(shadowTint, highlightTint, smoothstep(0.15, 2.0, y));

    vec3 mapped = aces(hdr);
    float d = distance(texCoord, vec2(0.5));
    float vignette = 1.0 - 0.14 * smoothstep(0.34, 0.72, d);
    mapped *= vignette;

    // Dither below one 8-bit step to suppress visible gradients after tone mapping.
    mapped += (hash12(gl_FragCoord.xy) - 0.5) / 255.0;
    fragColour = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
