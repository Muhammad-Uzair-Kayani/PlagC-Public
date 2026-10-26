#version 330 core
uniform sampler2D sceneTexture;
uniform float exposure;
in vec2 texCoord;
out vec4 fragColour;
void main(){
    vec3 x=max(texture(sceneTexture,texCoord).rgb*exposure,vec3(0.0));
    vec3 mapped=clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
    fragColour=vec4(mapped,1.0);
}
