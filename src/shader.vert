#version 450

layout(location = 0) out vec2 fragTexCoord;

// Two clockwise triangles forming a quad (matches VK_FRONT_FACE_CLOCKWISE + back-face culling).
vec2 positions[6] = vec2[](
    vec2(-0.8, -0.8), vec2( 0.8, -0.8), vec2( 0.8,  0.8),
    vec2( 0.8,  0.8), vec2(-0.8,  0.8), vec2(-0.8, -0.8)
);

vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(1.0, 1.0), vec2(0.0, 1.0), vec2(0.0, 0.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = uvs[gl_VertexIndex];
}