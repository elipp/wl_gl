#ifndef MESH_H
#define MESH_H

#include <map>
#include <lin_alg.h>

#include "wl_stuff.h"
#include "shader.h"

class mesh_t {
private:
	vec4 position;
	Quaternion orientation;
	mat4 modelview;

	GLuint VAOid;
	GLuint VBOid;
	
	int num_triangles;

	ShaderProgram *shader;

	GLuint texId;
	std::string name;
	bool bad;

public:
	bool is_bad() const { return bad; }
	void render();
	mesh_t(std::string filename, ShaderProgram *shader, GLuint texture_id);
	void use_modelview(const mat4 &mv) { modelview = mv; }

};


typedef std::map<GLuint, unsigned short> VBOid_numfaces_map_t;

#endif
