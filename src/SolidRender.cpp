/**
 * Name: Kishan S Patel
 * Email: kishan.patel@digipen.edu
 * Assignment Number: 3
 * Course: CS200
 * Term: Fall 2024
 *
 * File: SolidRender.cpp
 *
 * OpenGL Mesh Wrapper that also takes care of the shader program ot use & draw
 */

#include "SolidRender.h"
#include <array>
#include <iostream>

/**
 * @brief Fragment Shader Source
 */
constexpr const char *FRAGMENT_SOURCE = R"(
  #version 130

  uniform vec4 color;
  out vec4 frag_color;
  void main(void) {
    frag_color = color;
  }
)";

/**
 * @brief Vertex Shader Source
 */
constexpr const char *VERTEX_SOURCE = R"(
  #version 130

  in vec4 position;

  uniform mat4 transform;
  void main() {
    gl_Position = transform * position;
  }
)";

namespace cs200 {

  /**
   * @brief Compiles a shader of type 'shader_type' from source
   *
   * @param source Source code in text for the shader
   * @param shader_type OpenGL enum for what shader to use (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER typically)
   */
  static GLuint compile_shader(const char *const source, GLenum shader_type) {
    const GLuint shader_id = glCreateShader(shader_type);

    if (shader_id == 0) throw std::runtime_error{"Failure to create shader (possible incorrect shader_type)"};

    // upload shader source to the GPU
    glShaderSource(shader_id, 1, &source, nullptr);

    // compile
    glCompileShader(shader_id);

    // check for success
    int32_t success{0};
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (not success) {
      // if no sucess get the info log and panic
      std::array<char, 512> info_log{'\0'};
      glGetShaderInfoLog(shader_id, info_log.size(), nullptr, info_log.data());
      throw std::runtime_error{std::string{"Failed to compile Shader: "} + info_log.data()};
    }

    return shader_id;
  }

  /**
   * @brief Links two shaders together
   */
  static GLuint link_program(const GLuint vertex_shader, const GLuint fragment_shader) {
    const GLuint program_id = glCreateProgram();

    if (program_id == 0) throw std::runtime_error{"Failed to create program"};

    // attach shaders to the program
    glAttachShader(program_id, vertex_shader);
    glAttachShader(program_id, fragment_shader);

    glLinkProgram(program_id);

    // clean up shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // check for success
    int32_t success{0};
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);

    if (not success) {
      // get info log & panic if failure
      std::array<char, 512> info_log{'\0'};
      glGetProgramInfoLog(program_id, info_log.size(), nullptr, info_log.data());
      throw std::runtime_error{std::string{"Failed to compile link program: "} + info_log.data()};
    }

    return program_id;
  }

  template<typename T>
  static void create_vao_and_ebo(
      GLuint &vao,
      GLuint &ebo,
      const GLuint vertex_buffer,
      const GLuint attribute_position,
      const T *const index_data,
      const std::size_t indices_length) {

    // create the element buffer object for the indecies
    {
      // bind as the active EBO
      glGenBuffers(1, &ebo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

      // upload the data
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_length * sizeof(T), index_data, GL_STATIC_DRAW);

      // cleanup
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    {
      // create the vertex buffer object
      glGenVertexArrays(1, &vao);

      // bind it as current
      glBindVertexArray(vao);
    }

    // bind the vertex buffer to the VAO for the actual array data
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

    // bind the element indecies array
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    // setup the Vertex Attribute,
    {
      // one vertex is just composed of a vec4, so its just 4 floats with no offset at the beginning
      glVertexAttribPointer(attribute_position, 4, GL_FLOAT, false, sizeof(glm::vec4), nullptr);
      glEnableVertexAttribArray(attribute_position);
    }

    // cleanup
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  SolidRender::SolidRender() :
      ucolor{0}, //
      utransform{0}, //
      program{0}, //
      vao_edges{0}, //
      vao_faces{0}, //
      vertex_buffer{0}, //
      edge_buffer{0}, //
      face_buffer{0}, //
      mesh_edge_count{0}, //
      mesh_face_count{0} {

    program = link_program(
        compile_shader(VERTEX_SOURCE, GL_VERTEX_SHADER), //
        compile_shader(FRAGMENT_SOURCE, GL_FRAGMENT_SHADER) //
    );

    glUseProgram(program);
    ucolor = glGetUniformLocation(program, "color");
    utransform = glGetUniformLocation(program, "transform");
    glUseProgram(0);
  }

  SolidRender::~SolidRender() { glDeleteProgram(program); }

  void SolidRender::clearFrame(const glm::vec4 &c) {
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void SolidRender::setTransform(const glm::mat4 &M) { // NOLINT
    glUseProgram(program);
    glUniformMatrix4fv(utransform, 1, false, &M[0][0]);
  }

  void SolidRender::loadMesh(const Mesh &mesh) {

    // Create the VBO for the raw vertex data
    glGenBuffers(1, &vertex_buffer);

    // bind the VBO as current
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

    // upload the vertex data to the GPU
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLintptr>(mesh.vertexCount() * sizeof(glm::vec4)),
        mesh.vertexArray(),
        GL_STATIC_DRAW);

    // cleanup
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(program);
    const GLuint attribute_position = glGetAttribLocation(program, "position");

    // create the faces VAO
    mesh_face_count = mesh.faceCount();
    create_vao_and_ebo<Mesh::Face>(
        vao_faces, face_buffer, vertex_buffer, attribute_position, mesh.faceArray(), mesh_face_count);

    // create the edges VAO
    mesh_edge_count = mesh.edgeCount();
    create_vao_and_ebo<Mesh::Edge>(
        vao_edges, edge_buffer, vertex_buffer, attribute_position, mesh.edgeArray(), mesh_edge_count);

    glUseProgram(0);
  }

  void SolidRender::unloadMesh() {
    glDeleteVertexArrays(1, &vao_edges);
    glDeleteVertexArrays(1, &vao_faces);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &edge_buffer);
    glDeleteBuffers(1, &face_buffer);
  }

  void SolidRender::displayEdges(const glm::vec4 &c) /* const */ {
    // bind VAO for edges & do the draw call

    // set active program
    glUseProgram(program);
    // upload color uniform
    glUniform4f(ucolor, c.r, c.g, c.b, c.a);

    glBindVertexArray(vao_edges);
    glDrawElements(GL_LINES, mesh_edge_count * 2, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glUseProgram(0);
  }

  void SolidRender::displayFaces(const glm::vec4 &c) /* const */ {
    // bind VAO for faces & do the draw call

    glUseProgram(program);
    glUniform4f(ucolor, c.r, c.g, c.b, c.a);

    glBindVertexArray(vao_faces);

    glDrawElements(GL_TRIANGLES, mesh_face_count * 3, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);

    glUseProgram(0);
  }
} // namespace cs200
