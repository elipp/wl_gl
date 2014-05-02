#include <lin_alg.h>
#include <cassert>

#include "heightmap.h"
#include "texturewl.h"
#include "text.h"

HeightMap::HeightMap(const std::string &filename, float param_scale, float param_top, float param_bottom) 
	: scale(param_scale), top(param_top*param_scale), bottom(param_bottom*param_scale), real_map_dim(16 * param_scale) {
	
	// the heightmaps are 16-by-16 meshes, which are scaled by the _scale parm

	bad = false;
	img_info_t img_info;

	unsigned char *pixel_buffer;

	if (!load_pixels(filename, &pixel_buffer, &img_info)) {
		fprintf(stderr, "HeightMap: loading file %s pheyled.\n", filename.c_str()); 
		bad = true; 
		return; 
	}

	else if (img_info.bpp != 8) {
		fprintf(stderr, "HeightMap (input file \"%s\"): only 8-bit grayscale accepted! :(\n", filename.c_str());
		bad = true;
		return;
	}

	this->pixels = std::vector<unsigned char>(pixel_buffer, pixel_buffer + img_info.total_bytes());

	img_dim_pixels = img_info.width;

	real_map_dim_minus_one = real_map_dim - 1;
	
	dim_per_scale = img_dim_pixels/real_map_dim;
	half_real_map_dim = real_map_dim/2.0;
	dim_minus_one = img_dim_pixels-1;
	dim_squared_minus_one = img_dim_pixels*img_dim_pixels - 1;

	max_elevation_real_y = param_top * param_scale;
	min_elevation_real_y = param_bottom * param_scale;

	elevation_real_diff = max_elevation_real_y - min_elevation_real_y;

	PRINT("HeightMap (filename: %s):\n", filename.c_str());
	PRINT("img_dim_pixels = %d, dim_per_scale = %f, max_elevation_real_y = %f,\nmin_elevation_real_y = %f, elevation_real_diff = %f\n",
						img_dim_pixels, dim_per_scale, max_elevation_real_y, min_elevation_real_y, elevation_real_diff);
	PRINT("pixel buffer size: %u\n", this->pixels.size());

	assert(bad == false);

}

inline float HeightMap::get_pixel(int x, int y) {
		int index_x = dim_per_scale*CLAMP((double)(x+half_real_map_dim), (double)0, (double)real_map_dim_minus_one);
		int index_y = dim_per_scale*CLAMP((double)(y+half_real_map_dim), (double)0, (double)real_map_dim_minus_one);	
		index_y = dim_minus_one - index_y;
		return pixels[index_x + (index_y*img_dim_pixels)];
}

float HeightMap::lookup(float x, float y) {

	// perform bilinear interpolation on the heightmap

	int xi; 
	float xf, xf_r;
	xi = floor(x);
	xf = x - xi;
	xf_r = 1.0 - xf;
	
	int yi; 
	float yf, yf_r;
	yi = floor(y);
	yf = y - yi;
	yf_r = 1.0 - yf;
	
/*	// this is a simple scalar implementation
	float z11 = get_pixel(xi, yi);
	float z21 = get_pixel(xi+1, yi);
	float z12 = get_pixel(xi, yi+1);
	float z22 = get_pixel(xi+1, yi+1);

	float R1 = xf_r * z11 + xf * z21;
	float R2 = xf_r * z12 + xf * z22;
	float r = (yf_r * R1 + yf * R2)/255.0;  */
	
	vec4 z = _mm_setr_ps(get_pixel(xi, yi), 
				   get_pixel(xi+1, yi), 
				   get_pixel(xi, yi+1), 
				   get_pixel(xi+1, yi+1));

	vec4 w = _mm_setr_ps(xf_r * yf_r, 
				xf * yf_r, 
			  	xf_r * yf, 
			   	xf * yf);

	float r = dot4(z, w)/255.0;

	
	return min_elevation_real_y + r * elevation_real_diff;
//	return min_elevation_real_y + (get_pixel(xi, yi))*elevation_real_diff;
}



