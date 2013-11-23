#ifndef TEXTUREWL_H
#define TEXTUREWL_H

#define IS_POWER_OF_TWO(x) ((x) & ((x) - 1))

struct unified_header_data {
	unsigned width, height;
	int bpp;
};

int loadJPEG(const std::string &filename, unsigned char **out_buffer, unified_header_data *out_header, int *total_bytes);
int load_pixels(const std::string& filename, std::vector<unsigned char> &pixels, unified_header_data *img_info);

#endif
