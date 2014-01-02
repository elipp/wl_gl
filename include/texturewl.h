#ifndef TEXTUREWL_H
#define TEXTUREWL_H

#include <GLES2/gl2.h>
#include "image_loader.h"

int load_texture(const std::string &filename, GLuint *id, int filter);

#endif
