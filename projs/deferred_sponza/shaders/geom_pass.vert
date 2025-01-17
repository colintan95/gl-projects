#version 330 core
layout(location = 0) in vec3 vert_pos;
layout(location = 1) in vec3 vert_normal;
layout(location = 2) in vec2 vert_texcoord;
layout(location = 3) in uint vert_mtl_id;

out vec3 frag_pos;
out vec3 frag_normal;
out vec2 frag_texcoord;
flat out uint frag_mtl_id;

uniform mat4 mv_mat;
uniform mat4 mvp_mat;
uniform mat3 normal_mat;

void main() {
  frag_pos = (mv_mat * vec4(vert_pos, 1.0)).xyz;
  frag_normal = normal_mat * vert_normal;
  frag_texcoord = vert_texcoord;
  frag_mtl_id = vert_mtl_id;

  gl_Position = mvp_mat * vec4(vert_pos, 1.0);
}