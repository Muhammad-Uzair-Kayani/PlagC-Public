// Basic Colored Square Shader

#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
// location 1 (a_TexCoord) exists in your buffer, but we don't need it.

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec3 v_Color;

void main()
{
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);

    // Hardcoded color for each corner
    if (a_Position.x < 0.0 && a_Position.y < 0.0)
        v_Color = vec3(1.0, 0.0, 0.0); // Bottom-left: Red
    else if (a_Position.x >= 0.0 && a_Position.y < 0.0)
        v_Color = vec3(0.0, 1.0, 0.0); // Bottom-right: Green
    else if (a_Position.x >= 0.0 && a_Position.y >= 0.0)
        v_Color = vec3(0.0, 0.0, 1.0); // Top-right: Blue
    else
        v_Color = vec3(1.0, 1.0, 0.0); // Top-left: Yellow
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Color;

void main()
{
    color = vec4(v_Color, 1.0);
}