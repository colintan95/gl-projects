#version 330 core

layout(location = 0) in vec3 vert_pos;
layout(location = 1) in vec3 vert_normal;

out vec3 frag_pos;
out vec3 frag_normal;

uniform mat4 mv_mat;
uniform mat4 mvp_mat;
uniform mat3 normal_mat;

void main() {
  gl_Position = mvp_mat * vec4(vert_pos, 1.0);
  
  frag_pos = vec3(mv_mat * vec4(vert_pos, 1.0));
  frag_normal = normal_mat * vert_normal;
}