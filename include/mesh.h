#ifndef MESH_H
#define MESH_H

#include <map>
#include <GLES2/gl2.h>
#include <lin_alg.h>

class mesh_t {
private:
	vec4 position;
	Quaternion orientation;

	GLuint VAOid;
	GLuint VBOid;
	
	int num_triangles;

	GLuint shaderId;
	GLuint texId;
	std::string name;
	bool bad;

public:
	bool is_bad() const { return bad; }
	void render();
	mesh_t(std::string filename, GLuint shader_id, GLuint texture_id);

};

typedef std::map<GLuint, unsigned short> VBOid_numfaces_map_t;

#endif
