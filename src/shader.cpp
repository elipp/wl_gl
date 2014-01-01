#include "shader.h"
#include "text.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <streambuf>
#include <map>
#include <cassert>
#include <cstring>

static void update_uniform_mat4(GLuint location, const GLvoid* data) {
	glUniformMatrix4fv(location, 1, GL_FALSE, (const GLfloat*)data);
}

static void update_uniform_vec4(GLuint location, const GLvoid* data) {
	glUniform4fv(location, 1, (const GLfloat*)data);
}

static void update_uniform_1i(GLuint location, const GLvoid* data) {
	glUniform1i(location, (*(GLint*)data));
}

static void update_uniform_1f(GLuint location, const GLvoid* data) {
	glUniform1f(location, (*(GLfloat*)data));
}

static void update_uniform_sampler2D(GLuint location, const GLvoid* data) {
	update_uniform_1i(location, data);
}

static const char *attrib_Position = "attrib_Position";
static const char *attrib_Normal = "attrib_Normal";
static const char *attrib_TexCoord = "attrib_TexCoord";

#define BIND_ATTRIB_REPORT(program_id, location, attrib_string) do {\
 glBindAttribLocation((program_id), (location), attrib_string); \
PRINT("shader %u: bound attrib \"%s\" to location %d (%s)\n", program_id, attrib_string, location, #location);\
fprintf(stderr, "shader %u: bound attrib \"%s\" to location %d (%s)\n", program_id, attrib_string, location, #location);\
} while(0)

static void setup_attributes_V3N3T2(GLuint shader_program_id) {
	BIND_ATTRIB_REPORT(shader_program_id, ATTRIB_POSITION, attrib_Position);
	BIND_ATTRIB_REPORT(shader_program_id, ATTRIB_NORMAL, attrib_Normal);
	BIND_ATTRIB_REPORT(shader_program_id, ATTRIB_TEXCOORD, attrib_TexCoord);
}

static void setup_attributes_V2T2(GLuint shader_program_id) {
	BIND_ATTRIB_REPORT(shader_program_id, TEXT_ATTRIB_POSITION, attrib_Position);
	BIND_ATTRIB_REPORT(shader_program_id, TEXT_ATTRIB_TEXCOORD, attrib_TexCoord);	
}

static GLuint create_shader(const std::string &filename, GLenum shader_type) {
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	std::ifstream in(filename);
	if (!in.is_open()) {
		fprintf(stderr, "fatal error: couldn't open shader file \"%s\".\n", filename.c_str());
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

static UNIFORM_UPDATE_FUNCP get_uniform_update_function(const std::string &uniform_name) {

	size_t prefix_end_pos = strchr(uniform_name.c_str(), '_') - uniform_name.c_str();
	const std::string uniform_name_prefix = uniform_name.substr(0, prefix_end_pos);
	if (uniform_name_prefix == "mat4") {
		return update_uniform_mat4;

	} else if (uniform_name_prefix == "vec4") {
		return update_uniform_vec4;

	} else if (uniform_name_prefix == "sampler2D") {
		return update_uniform_sampler2D;

	} else if (uniform_name_prefix == "int") {
		return update_uniform_1i;

	} else if (uniform_name_prefix == "float") {
		return update_uniform_1f;

	} else {
		fprintf(stderr, "get_uniform_type: error: GLSL uniforms must be prefixed with the exact GLSL type name and an underscore. ex: mat4_ModelView. (got \"%s\")\n", uniform_name_prefix.c_str());
		return NULL;
	}
}

void ShaderProgram::construct_uniform_map() {
	GLint total = 0;
	glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &total);

#define UNIFORM_NAME_LEN_MAX 64	// should really be queried with GL_ACTIVE_UNIFORM_LENGTH_MAX

	char uniform_name_buf[UNIFORM_NAME_LEN_MAX];
	memset(uniform_name_buf, 0, UNIFORM_NAME_LEN_MAX);

	for (GLint i = 0; i < total; i++) {
		GLsizei name_len = 0;
		GLint num = 0;
		GLenum type = GL_ZERO;
		glGetActiveUniform(program_id, i, sizeof(uniform_name_buf) - 1, &name_len, &num, &type, uniform_name_buf);
		uniform_name_buf[name_len] = '\0';

		uniform_location_type_pair p;
		p.location = glGetUniformLocation(program_id, uniform_name_buf);
		p.uniform_update_func = get_uniform_update_function(uniform_name_buf);
		assert(p.uniform_update_func != NULL);

		uniforms.insert(std::pair<std::string, uniform_location_type_pair>(std::string(uniform_name_buf), p));

		PRINT("construct_uniform_map: shader %s: inserted uniform %s with location %d.\n", id_string.c_str(), uniform_name_buf, p.location);
	}
}

uniform_location_type_pair* ShaderProgram::get_uniform_location_type_pair(const std::string &name) {
	auto iter = uniforms.find(name);
	if (iter == uniforms.end()) {
		return NULL;
	}
	else return &iter->second;
}


ShaderProgram::ShaderProgram(const std::string &folder_path, int SHADER_ATTRIB_FORMAT) {

	GLuint fs, vs;
	GLint status;

	fs = create_shader(folder_path + "/fs", GL_FRAGMENT_SHADER);
	vs = create_shader(folder_path + "/vs", GL_VERTEX_SHADER);

	id_string = folder_path;

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

	static void (*ATTRIB_SETUP_FUNCTIONS[2])(GLuint) = { setup_attributes_V3N3T2, setup_attributes_V2T2 };
	ATTRIB_SETUP_FUNCTIONS[SHADER_ATTRIB_FORMAT](this->program_id);

	construct_uniform_map();

	glUseProgram(0);

}

void ShaderProgram::update_uniform(const std::string &name, const GLvoid* data) {

//	fprintf(stderr, "calling update_uniform for uniform \"%s\"\n", name.c_str());
	auto iter = uniforms.find(name);
	if (iter == uniforms.end()) {
		fprintf(stderr, "ShaderProgram::update_uniform: shader %s: warning! uniform \"%s\" not active in shader program.\n", id_string.c_str(), name.c_str());

		return;
	}
	
	GLuint uniform_location = iter->second.location;
	glUseProgram(this->program_id);
	iter->second.uniform_update_func(uniform_location, data);

}

void ShaderProgram::update_sampler2D(const std::string &sampler2D_name, GLuint value) {
	auto iter = uniforms.find(sampler2D_name);
	if (iter == uniforms.end()) {
//		fprintf(stderr, "ShaderProgram::update_uniform: shader %s: warning! uniform \"%s\" not active in shader program.\n", id_string.c_str(), sampler2D_name.c_str());

		return;
	}

	glUseProgram(this->program_id);
	glUniform1i(iter->second.location, value);
	
}

ShaderProgram::~ShaderProgram() {
	glDeleteProgram(this->program_id);
}

