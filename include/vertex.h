#ifndef VERTEX_H
#define VERTEX_H

/**
 *	For VBOs, the OpenGL-wiki suggests the vertex struct be a multiple of 32 bytes in size (ATI specific, for NVidia doesn't matter, apparently)
 */

typedef struct vertex_ {

		float vx,vy,vz;
		float nx,ny,nz;
		float u,v;

} vertex;


#endif
