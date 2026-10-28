#version ogre_glsl_ver_330
#ifdef PLAGC_VULKAN_SHADER
#define PC_UNIFORM
#else
#define PC_UNIFORM uniform
#endif
vulkan(layout(ogre_P0) uniform Params {)
PC_UNIFORM int mode;
PC_UNIFORM float threshold;
vulkan(};)
vulkan_layout(ogre_t0) uniform texture2D sourceTexture;
vulkan(layout(ogre_s0) uniform sampler sourceTextureSampler);
vulkan_layout(ogre_t1) uniform texture2D lowTexture;
vulkan(layout(ogre_s1) uniform sampler lowTextureSampler);
vulkan_layout(location = 0) in vec2 texCoord;
vulkan_layout(location = 0) out vec4 fragColour;
vec3 tap(vec2 uv) { return max(texture(vkSampler2D(sourceTexture, sourceTextureSampler), clamp(uv, vec2(0), vec2(1))).rgb,vec3(0)); }
void main()
{
    vec2 px=1.0/vec2(textureSize(vkSampler2D(sourceTexture, sourceTextureSampler),0));
    vec3 c;
    if(mode==0 || mode==1)
    {
        c=(tap(texCoord+px*vec2(-.5,-.5))+tap(texCoord+px*vec2(.5,-.5))+
           tap(texCoord+px*vec2(-.5,.5))+tap(texCoord+px*vec2(.5,.5)))*.25;
        if(mode==0)
        {
            float brightness=max(max(c.r,c.g),c.b);
            float knee=max(.01,threshold*.5);
            float soft=clamp(brightness-threshold+knee,0.0,2.0*knee);
            soft=soft*soft/(4.0*knee);
            c*=max(brightness-threshold,soft)/max(brightness,.0001);
        }
    }
    else if(mode==2 || mode==3)
    {
        vec2 dir=mode==2?vec2(px.x,0):vec2(0,px.y);
        c=tap(texCoord)*.227027;
        c+=(tap(texCoord+dir*1.384615)+tap(texCoord-dir*1.384615))*.316216;
        c+=(tap(texCoord+dir*3.230769)+tap(texCoord-dir*3.230769))*.070270;
    }
    else
    {
        // Upsample and combine at half resolution; scene stays in linear HDR until tone mapping.
        vec2 p=1.0/vec2(textureSize(vkSampler2D(lowTexture, lowTextureSampler),0));
        vec3 low=texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord).rgb*4.0;
        low+=(texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord+p*vec2(1,0)).rgb+
              texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord-p*vec2(1,0)).rgb+
              texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord+p*vec2(0,1)).rgb+
              texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord-p*vec2(0,1)).rgb)*2.0;
        low+=texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord+p).rgb+texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord-p).rgb+
             texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord+p*vec2(1,-1)).rgb+texture(vkSampler2D(lowTexture, lowTextureSampler),texCoord+p*vec2(-1,1)).rgb;
        c=tap(texCoord)*.35+low*(.65/16.0);
    }
    fragColour=vec4(c,1);
}
