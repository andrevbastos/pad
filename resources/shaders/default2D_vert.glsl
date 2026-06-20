#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

out vec4 ourColor;

uniform mat4 model;
uniform mat4 cam;

void main()
{
    gl_Position = cam * model * vec4(aPos, -1.0, 1.0);
    ourColor = aColor;
}