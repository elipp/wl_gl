#include <iostream>
#include <fstream>
#include <cstring>

#include "mesh.h"
#include "lzma/mylzma.h"
#include "wl_stuff.h"
#include "vertex.h"

extern GLuint ModelView_u;
extern GLuint Projection_u;
extern GLuint sampler0_u;

extern mat4 View;
extern mat4 Projection;

static int loadlzmabobj(const std::string &filename, GLuint *VBOid, int *num_triangles) {

	std::ifstream infile(filename.c_str(), std::ios::binary);

	if (!infile.is_open()) { 
		fprintf(stderr, "[loadlzmabobj]: failed loading model file \"%s\"\n", filename.c_str()); 
		return 0;
	}

	fprintf(stderr, "[lzmabobjloader]: loading file %s...\n", filename.c_str());

	char *decompressed;
	size_t decompressed_size;

	int res = LZMA_decode(filename, &decompressed, &decompressed_size);	// this also allocates the memory
	if (res != 0) {
		fprintf(stderr, "LZMA decoder: an error occurred. Aborting.\n"); 
		return 0;
	}

	if (strncmp("bobj", decompressed, 4) != 0) {
		fprintf(stderr, "[lzmabobjloader]: error: unrecognized bobj header.\n");
		return 0;
	}

	char* iter = decompressed + 4;	// the first 4 bytes of a bobj file contain the letters "bobj". Or do they? :D

	// read vertex & face count.

	unsigned int vcount;
	memcpy(&vcount, iter, sizeof(vcount));

	int total_facecount = vcount / 3;
	*num_triangles = total_facecount;

	iter = decompressed + 8;

	glGenBuffers(1, VBOid);
	glBindBuffer(GL_ARRAY_BUFFER, *VBOid);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex)*vcount, iter, GL_STATIC_DRAW);

	fprintf(stderr, "[lzmabobjloader]: Successfully loaded LZMA-compressed bobjfile \"%s\" with %d vertices.\n\n", filename.c_str(), (int)vcount);
	delete [] decompressed;	// not needed anymore

	glDisableVertexAttribArray(ATTRIB_POSITION);
	glDisableVertexAttribArray(ATTRIB_NORMAL);
	glDisableVertexAttribArray(ATTRIB_TEXCOORD);


	return 1;
}

void mesh_t::render() {
	

	glEnable(GL_DEPTH_TEST);
	glUseProgram(shader->program_id);

	shader->update_uniform("mat4_ModelView", (const GLvoid*)this->modelview.rawData());
	shader->update_uniform("mat4_Projection", (const GLvoid*)Projection.rawData());

	if (this->textured) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->texId);
		shader->update_sampler2D("sampler2D_texture0", 0);
	}

	my_glBindVertexArray(this->VAOid);
	glDrawArrays(GL_TRIANGLES, 0, this->num_triangles*3);

	glUseProgram(0);
	my_glBindVertexArray(0);
	
}

mesh_t::mesh_t(std::string filename, ShaderProgram *shader_program, bool is_textured, GLuint texture_id)
	: shader(shader_program), textured(is_textured), texId(texture_id) {

	this->bad = true;

	if (!loadlzmabobj(filename, &this->VBOid, &this->num_triangles)) {
		this->bad = true;
		return;
	}

	this->position = vec4(0.0, 0.0, 0.0, 1.0);

	my_glGenVertexArrays(1, &this->VAOid);
	my_glBindVertexArray(this->VAOid);

	glBindBuffer(GL_ARRAY_BUFFER, this->VBOid);

	glEnableVertexAttribArray(ATTRIB_POSITION);
	glEnableVertexAttribArray(ATTRIB_NORMAL);
	glEnableVertexAttribArray(ATTRIB_TEXCOORD);

	glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(0));
	glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(3*sizeof(float)));
	glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(6*sizeof(float)));

	my_glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	this->bad = false;
}



VBOid_numfaces_map_t generate_VBOid_numfaces_map(int total_facecount) {

	VBOid_numfaces_map_t map;
#define NUM_FACES_PER_VBO_MAX ((0xFFFF)/3)
	int num_VBOs = total_facecount/NUM_FACES_PER_VBO_MAX + 1;

	unsigned num_excess_last = total_facecount%NUM_FACES_PER_VBO_MAX;

	fprintf(stderr, "# faces = %d => num_VBOs = %d, num_excess_last = %d\n", total_facecount, num_VBOs, num_excess_last);
	GLuint *VBOids = new GLuint[num_VBOs];
	glGenBuffers(num_VBOs, VBOids);

	int i = 0;
	for (i = 0; i < num_VBOs-1; ++i) {
		map.insert(std::pair<GLuint, unsigned short>(VBOids[i], NUM_FACES_PER_VBO_MAX));
	}

	map.insert(std::pair<GLuint,unsigned short>(VBOids[i], num_excess_last));
	delete [] VBOids;


	return map;

}

void populate_VBOid_numfaces_map(VBOid_numfaces_map_t &m, const vertex *vertices) {

	size_t offset = 0;
	for (auto &iter : m) {
		const GLuint &current_VBOid = iter.first;
		const unsigned short &current_num_faces = iter.second;
		glBindBuffer(GL_ARRAY_BUFFER, current_VBOid);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex)*current_num_faces*3, &vertices[offset].vx, GL_STATIC_DRAW);
		offset += NUM_FACES_PER_VBO_MAX*3;
	}
	
}

void render_VBOid_numfaces_map(const VBOid_numfaces_map_t &map, GLuint IBO_id) {

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO_id);

	for (auto &iter : map) {
		const GLuint &current_VBOid = iter.first;
		const unsigned short &num_faces = iter.second;
		glBindBuffer(GL_ARRAY_BUFFER, current_VBOid);
		glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(0));
		glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(3*sizeof(float)));
		glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(6*sizeof(float)));
		glDrawElements(GL_TRIANGLES, num_faces*3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
	}

}


