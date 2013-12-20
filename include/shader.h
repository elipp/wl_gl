#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <unordered_map>

#include "wl_stuff.h"

enum { ATTRIB_POSITION = 0, ATTRIB_NORMAL, ATTRIB_TEXCOORD };

typedef void (*UNIFORM_UPDATE_FUNCP)(GLuint, const GLvoid*);

struct uniform_location_type_pair {
	GLuint location;
	UNIFORM_UPDATE_FUNCP uniform_update_func;
};

void update_uniform_mat4(GLuint location, const GLvoid* data);
void update_uniform_vec4(GLuint location, const GLvoid* data);
void update_uniform_1i(GLuint location, const GLvoid* data);
void update_uniform_1f(GLuint location, const GLvoid* data);
void update_uniform_sampler2D(GLuint location, const GLvoid* data);

typedef std::unordered_map<std::string, uniform_location_type_pair> uniform_map_t;

class ShaderProgram {

public:
	std::string id_string;
	uniform_map_t uniforms;	// uniform name -> uniform location & type
	GLuint program_id;

	void update_uniform(const std::string &name, const GLvoid* data);
	uniform_location_type_pair* get_uniform_location_type_pair(const std::string &name);
	void construct_uniform_map();
	ShaderProgram(const std::string &folder_path);
	~ShaderProgram();

};


#endif
