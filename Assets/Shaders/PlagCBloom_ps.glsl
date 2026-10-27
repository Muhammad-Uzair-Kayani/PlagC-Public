#version 330 core
uniform sampler2D sourceTexture;
uniform sampler2D lowTexture;
uniform int mode;
uniform float threshold;
in vec2 texCoord;
out vec4 fragColour;
vec3 tap(vec2 uv) { return max(texture(sourceTexture, clamp(uv, vec2(0), vec2(1))).rgb,vec3(0)); }
void main()
{
    vec2 px=1.0/vec2(textureSize(sourceTexture,0));
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
        vec2 p=1.0/vec2(textureSize(lowTexture,0));
        vec3 low=texture(lowTexture,texCoord).rgb*4.0;
        low+=(texture(lowTexture,texCoord+p*vec2(1,0)).rgb+
              texture(lowTexture,texCoord-p*vec2(1,0)).rgb+
              texture(lowTexture,texCoord+p*vec2(0,1)).rgb+
              texture(lowTexture,texCoord-p*vec2(0,1)).rgb)*2.0;
        low+=texture(lowTexture,texCoord+p).rgb+texture(lowTexture,texCoord-p).rgb+
             texture(lowTexture,texCoord+p*vec2(1,-1)).rgb+texture(lowTexture,texCoord+p*vec2(-1,1)).rgb;
        c=tap(texCoord)*.35+low*(.65/16.0);
    }
    fragColour=vec4(c,1);
}
