#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include <vector>
#include "common.h"

class HeightMap {
	int img_dim_pixels;	// pixels
	int bitdepth;
	std::vector<unsigned char> pixels;
	bool bad;

	float scale; // units
	float top;	// maximum elevation
	float bottom; // minimum elevation
	float real_map_dim;
	double dim_per_scale;
	double half_real_map_dim;
	float real_map_dim_minus_one;
	int dim_minus_one;
	int dim_squared_minus_one;
	float max_elevation_real_y;
	float min_elevation_real_y;
	float elevation_real_diff;

	inline float get_pixel(int x, int y);

public:
	bool is_bad() const { return bad; }
	float lookup(float x, float y);
	HeightMap(const std::string &filename, float param_scale, float param_top, float param_bottom);
	HeightMap() {};

};



#endif
