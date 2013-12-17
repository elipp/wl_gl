#include "shader.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <streambuf>
#include <map>
#include <cassert>

static GLuint create_shader(const std::string &filename, GLenum shader_type) {
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	std::ifstream in(filename);
	if (!in.is_open()) {
		std::cerr << "Couldn't open shader file.\n";
		exit(1);
	}

	std::string file_contents;
	in.seekg(0, std::ios::end);
	file_contents.reserve(in.tellg());
	in.seekg(0, std::ios::beg);

	file_contents.assign((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));

	const GLchar *t = file_contents.c_str();

	glShaderSource(shader, 1, &t, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "GLSL shader error (filename: %s) compiling %s shader: %*s\n",
				filename.c_str(), shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
				len, log);
		exit(1);
	}

	return shader;
}

ShaderProgram::ShaderProgram(const std::string &folder_path) {

	GLuint fs, vs;
	GLint status;

	fs = create_shader(folder_path + "/fs", GL_FRAGMENT_SHADER);
	vs = create_shader(folder_path + "/vs", GL_VERTEX_SHADER);

	this->program_id = glCreateProgram();
	glAttachShader(this->program_id, fs);
	glAttachShader(this->program_id, vs);
	glLinkProgram(this->program_id);

	glGetProgramiv(this->program_id, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(this->program_id, 1000, &len, log);
		fprintf(stderr, "GLSL ES shader link error: log:\n%*s\n", len, log);
		exit(1);
	}
	
	glDetachShader(this->program_id, fs);
	glDetachShader(this->program_id, vs);

	glDeleteShader(fs);
	glDeleteShader(vs);

	glUseProgram(this->program_id);

	glBindAttribLocation(this->program_id, ATTRIB_POSITION, "attrib_Position");
	glBindAttribLocation(this->program_id, ATTRIB_NORMAL, "attrib_Normal");
	glBindAttribLocation(this->program_id, ATTRIB_TEXCOORD, "attrib_TexCoord");
	
	ModelView_uniform_location = glGetUniformLocation(this->program_id, "ModelView");
	Projection_uniform_location = glGetUniformLocation(this->program_id, "Projection");
	sampler0_uniform_location = glGetUniformLocation(this->program_id, "texture0");

	glUseProgram(0);

}

ShaderProgram::~ShaderProgram() {
	glDeleteProgram(this->program_id);
}

