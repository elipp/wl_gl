#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <string>
#include <vector>

struct img_info_t {
	int width, height;
	int bpp;
	int num_channels;
	long total_bytes() const { return (long)width*(long)height*(long)num_channels*((long)bpp/8); }
};

int load_pixels(const std::string &filename, unsigned char **buffer, img_info_t *img_data);

#endif
