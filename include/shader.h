#ifndef SHADER_H
#define SHADER_H

#include <string>
#include "wl_stuff.h"

class ShaderProgram {

public:
	GLuint program_id;
	GLuint ModelView_uniform_location;
	GLuint Projection_uniform_location;
	GLuint sampler0_uniform_location;

	ShaderProgram(const std::string &folder_path);
	~ShaderProgram();

};


#endif
