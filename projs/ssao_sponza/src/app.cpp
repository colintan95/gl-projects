#include "app.h"

#include <iostream>
#include <cstdlib>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "gfx_utils/primitives.h"

//
// Significant portion of implementation adapted from: LearnOpenGL
//   https://learnopengl.com/Advanced-Lighting/SSAO
//

static const float kPi = 3.14159265358979323846f;

static const int kWindowWidth = 1920;
static const int kWindowHeight = 1080;

static const std::string kGeomPassVertShaderPath = "shaders/geom_pass.vert";
static const std::string kGeomPassFragShaderPath = "shaders/geom_pass.frag";

static const std::string kSSAOPassVertShaderPath = "shaders/ssao_pass.vert";
static const std::string kSSAOPassFragShaderPath = "shaders/ssao_pass.frag";

// Format of vertex - {pos_x, pos_y, pos_z, texcoord_u, texcoord_v}
static const float kQuadVertices[] = {
  -1.f,  1.f, 0.f, 0.f, 1.f,
  -1.f, -1.f, 0.f, 0.f, 0.f,
   1.f,  1.f, 0.f, 1.f, 1.f,
   1.f, -1.f, 0.f, 1.f, 0.f
};

void App::Run() {
  Startup();

  MainLoop();

  Cleanup();
}

void App::MainLoop() {
  bool should_quit = false;

  while (!should_quit) {
    // ShadowPass();

    // LightPass();

    GeometryPass();

    SSAOPass();

    window_.SwapBuffers();
    window_.TickMainLoop();

    if (window_.ShouldQuit()) {
      should_quit = true;
    }
  }
}

void App::GeometryPass() {
  glUseProgram(geom_pass_program_.GetProgramId());
  glViewport(0, 0, kWindowWidth, kWindowHeight);
  
  glBindFramebuffer(GL_FRAMEBUFFER, gbuf_fbo_);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBindVertexArray(geom_pass_vao_);

  glm::mat4 view_mat = camera_.CalcViewMatrix();
  glm::mat4 proj_mat = glm::perspective(glm::radians(30.f),
                                        window_.GetAspectRatio(),
                                        0.1f, 1000.f);    

  const auto& entities = scene_.GetEntities();                                

  for (auto entity_ptr : entities) {
    if (!entity_ptr->HasModel()) {
      continue;
    }

    for (auto& mesh : entity_ptr->GetModel()->GetMeshes()) {
      glm::mat4 model_mat = entity_ptr->ComputeTransform();
      glm::mat4 mv_mat = view_mat * model_mat;
      glm::mat4 mvp_mat = proj_mat * view_mat * model_mat;
      glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(mv_mat)));
      
      geom_pass_program_.GetUniform("mv_mat").Set(mv_mat);
      geom_pass_program_.GetUniform("mvp_mat").Set(mvp_mat);
      geom_pass_program_.GetUniform("normal_mat").Set(normal_mat);

      // Set vertex attributes

      GLuint pos_vbo_id = 
          resource_manager_.GetMeshVboId(mesh.id, 
                                         gfx_utils::kVertTypePosition);
      glBindBuffer(GL_ARRAY_BUFFER, pos_vbo_id);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

      GLuint normal_vbo_id =
          resource_manager_.GetMeshVboId(mesh.id, 
                                         gfx_utils::kVertTypeNormal);
      glBindBuffer(GL_ARRAY_BUFFER, normal_vbo_id);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

      GLuint texcoord_vbo_id =
          resource_manager_.GetMeshVboId(mesh.id, 
                                         gfx_utils::kVertTypeTexcoord);
      glBindBuffer(GL_ARRAY_BUFFER, texcoord_vbo_id);
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

      glDrawArrays(GL_TRIANGLES, 0, mesh.num_verts);
    }
  }

  glBindVertexArray(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::SSAOPass() {
  glUseProgram(ssao_pass_program_.GetProgramId());
  glViewport(0, 0, kWindowWidth, kWindowHeight);

  glBindVertexArray(ssao_pass_vao_);

  // glClear(GL_COLOR_BUFFER_BIT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for (int i = 0; i < 64; ++i) {
    ssao_pass_program_.GetUniform("samples", i).Set(ssao_kernel_[i]);
  }

  glm::mat4 proj_mat = glm::perspective(glm::radians(30.f),
                                        window_.GetAspectRatio(),
                                        0.1f, 1000.f);   

  ssao_pass_program_.GetUniform("proj_mat").Set(proj_mat);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gbuf_pos_tex_);
  ssao_pass_program_.GetUniform("pos_tex").Set(0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gbuf_normal_tex_);         
  ssao_pass_program_.GetUniform("normal_tex").Set(1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, ssao_noise_tex_);      
  ssao_pass_program_.GetUniform("noise_tex").Set(2);

  glBindBuffer(GL_ARRAY_BUFFER, ssao_pass_quad_vbo_);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                        (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                        (void*)(3 * sizeof(float)));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glBindVertexArray(0);
}

void App::Startup() {
  if (!window_.Inititalize(kWindowWidth, kWindowHeight, "Shadow Map")) {
    std::cerr << "Failed to initialize gfx window" << std::endl;
    exit(1);
  }

  if (!camera_.Initialize(&window_)) {
    std::cerr << "Failed to initialize camera" << std::endl;
    exit(1);
  }

  scene_.LoadSceneFromJson("assets/scene.json");

  resource_manager_.SetScene(&scene_);

  resource_manager_.CreateGLResources();

  lights_ = scene_.GetLightsByType<gfx_utils::PointLight>();

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);

  if (!geom_pass_program_.CreateFromFiles(kGeomPassVertShaderPath, 
                                          kGeomPassFragShaderPath)) {
    std::cerr << "Could not create geometry pass program." << std::endl;
    exit(1);
  }
  glUseProgram(geom_pass_program_.GetProgramId());

  glGenVertexArrays(1, &geom_pass_vao_);

  glGenFramebuffers(1, &gbuf_fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, gbuf_fbo_);

  glGenTextures(1, &gbuf_pos_tex_);
  glBindTexture(GL_TEXTURE_2D, gbuf_pos_tex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, kWindowWidth, kWindowHeight, 0, 
               GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
                         gbuf_pos_tex_, 0);

  glGenTextures(1, &gbuf_normal_tex_);
  glBindTexture(GL_TEXTURE_2D, gbuf_normal_tex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, kWindowWidth, kWindowHeight, 0, 
               GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 
                         gbuf_normal_tex_, 0);

  glGenTextures(1, &gbuf_albedo_spec_tex_);
  glBindTexture(GL_TEXTURE_2D, gbuf_albedo_spec_tex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWindowWidth, kWindowHeight, 0, 
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, 
                         gbuf_albedo_spec_tex_, 0);

  GLuint gbuf_attachments[3] = {
    GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
  };
  glDrawBuffers(3, gbuf_attachments);

  glGenRenderbuffers(1, &gbuf_depth_rbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, gbuf_depth_rbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, kWindowWidth, 
                        kWindowHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                            GL_RENDERBUFFER, gbuf_depth_rbo_);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer not complete!" << std::endl;
  }
  
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (!ssao_pass_program_.CreateFromFiles(kSSAOPassVertShaderPath, 
                                          kSSAOPassFragShaderPath)) {
    std::cerr << "Could not create ssao pass program." << std::endl;
    exit(1);
  }
  glUseProgram(ssao_pass_program_.GetProgramId());

  glGenVertexArrays(1, &ssao_pass_vao_);

  glGenBuffers(1, &ssao_pass_quad_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, ssao_pass_quad_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), &kQuadVertices, 
               GL_STATIC_DRAW);

  glUseProgram(0);

  std::uniform_real_distribution<GLfloat> random_floats(0.f, 1.f);
  std::default_random_engine generator;

  // Create sample kernel

  for (int i = 0; i < 64; ++i) {
    glm::vec3 sample(random_floats(generator) * 2.f - 1.f,
                     random_floats(generator) * 2.f - 1.f,
                     random_floats(generator));
    sample = glm::normalize(sample);
    sample *= random_floats(generator);

    float scale = static_cast<float>(i) / 64.f;
    scale = 0.1f + (scale * scale) * (1.f - 0.1f);

    sample *= scale;
    ssao_kernel_.push_back(sample);
  }

  // Create noise texture

  std::vector<glm::vec3> ssao_noise;

  for (int i = 0; i < 16; ++i) {
    glm::vec3 noise(random_floats(generator) * 2.f - 1.f,
                    random_floats(generator) * 2.f - 1.f,
                    0.f);
    ssao_noise.push_back(noise);
  }

  glGenTextures(1, &ssao_noise_tex_);
  glBindTexture(GL_TEXTURE_2D, ssao_noise_tex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, 
               &ssao_noise[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void App::Cleanup() {
  glDeleteTextures(1, &ssao_noise_tex_);

  glDeleteVertexArrays(1, &ssao_pass_vao_);

  glDeleteBuffers(1, &ssao_pass_quad_vbo_);

  ssao_pass_program_.Destroy();

  glDeleteRenderbuffers(1, &gbuf_depth_rbo_);

  glDeleteTextures(1, &gbuf_albedo_spec_tex_);
  glDeleteTextures(1, &gbuf_normal_tex_);
  glDeleteTextures(1, &gbuf_pos_tex_);

  glDeleteFramebuffers(1, &gbuf_fbo_);

  glDeleteVertexArrays(1, &geom_pass_vao_);

  geom_pass_program_.Destroy();

  resource_manager_.Cleanup();

  window_.Destroy();
}