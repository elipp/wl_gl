#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "texturewl.h"
#include "image_loader.h"
#include "jpeglib.h"
#include "png.h"

#define IS_POWER_OF_TWO(x) ((x) & ((x) - 1))

int load_texture(const std::string &filename, GLuint *id, int filter) {
	unsigned char* pixel_buf;
	img_info_t img_info;

	if (!load_pixels(filename, &pixel_buf, &img_info)) { 
		fprintf(stderr, "[load_texture]: load_pixels failed!\n");
		return 0;
	}

	glGenTextures(1, id);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *id);

	GLint colortype;

	switch (img_info.num_channels) {
		case 3:
			colortype = GL_RGB;
			break;
		case 4:
			colortype = GL_RGBA;
			break;
		default:
			fprintf(stderr, "load_texture: error: invalid number of channels in image resource file (%d)\n", img_info.num_channels);
			return 0;

	}

	glTexImage2D(GL_TEXTURE_2D, 0, colortype, img_info.width, img_info.height, 0, colortype, GL_UNSIGNED_BYTE, pixel_buf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);

	glGenerateMipmap(GL_TEXTURE_2D);

	free(pixel_buf);

	return 1;

}


