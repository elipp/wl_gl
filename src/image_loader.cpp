#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "jpeglib.h"
#include "png.h"
#include "image_loader.h"

static int loadJPEG(const std::string &filename, unsigned char **out_buffer, img_info_t *out_header);
static int loadPNG(const std::string &filename, unsigned char **out_buffer, img_info_t *out_header);

static std::string get_file_extension(const std::string &filename) {
	size_t i = 0;
	size_t fn_len = filename.length();
	while (i < fn_len && filename[i] != '.') {
		++i;
	}

	if (i == fn_len) { return filename; }

	std::string ext = filename.substr(i, filename.length() - i);
	return ext;
}

int load_pixels(const std::string& filename, unsigned char **pixel_buf, img_info_t *img_info) {

	memset(img_info, 0x0, sizeof(*img_info));

	std::string ext = get_file_extension(filename);
	if (ext == ".jpg" || ext == ".jpeg") {
		if (!loadJPEG(filename, pixel_buf, img_info)) {
			fprintf(stderr, "[load_pixels]: fatal error: loading file %s failed.\n", filename.c_str());
			return 0;
		}
	
	}
	else if (ext == ".png") {
		if(!loadPNG(filename, pixel_buf, img_info)) {
			fprintf(stderr, "[load_pixels]: fatal error: loading file %s failed.\n", filename.c_str());
			return 0;
		}
	} else {
		fprintf(stderr, "[load_pixels]: fatal error: unsupported image file extension \"%s\" (only .png, .jpg, .jpeg are supported)\n", ext.c_str());
		return 0;
	}

	return 1;

}

static int loadJPEG(const std::string &filename, unsigned char **out_buffer, img_info_t *out_header) {

	struct jpeg_decompress_struct cinfo;

	struct jpeg_error_mgr jerr;
	FILE* infile;		
	JSAMPROW *buffer;
	int row_stride;	

	infile = fopen(filename.c_str(), "rb");
	if (!infile) {
		fprintf(stderr, "loadJPEG: fatal: can't open %s\n", filename.c_str());
		return 0;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	jpeg_stdio_src(&cinfo, infile);

	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);

	out_header->width = cinfo.image_width;
	out_header->height = cinfo.image_height;
	out_header->bpp = 8;	
	out_header->num_channels = cinfo.output_components;

	row_stride = cinfo.output_width * cinfo.output_components;
	long total_bytes = out_header->total_bytes();

	*out_buffer = (unsigned char*)malloc(total_bytes);
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	unsigned char* outbuf_iter = *out_buffer;
	while (cinfo.output_scanline < cinfo.output_height) {
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);
		memcpy(outbuf_iter, buffer[0], row_stride);
		outbuf_iter += row_stride;
	}

	(void) jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	if (out_header->total_bytes() != total_bytes) { fprintf(stderr, "WARNING: loadJPEG: total_bytes() != total_bytes!! (%lld != %lld) \n", 
							(long long)out_header->total_bytes(), (long long)total_bytes); }

	fclose(infile);
	return 1;

}



static int loadPNG(const std::string &filename, unsigned char **out_buffer, img_info_t *out_header) {

	png_structp png_ptr;
	png_infop info_ptr;

	png_byte header[8];    

	FILE *fp = fopen(filename.c_str(), "rb");
	if (!fp) {
		fprintf(stderr, "[loadPNG] file %s could not be opened for reading", filename.c_str());
		return 0;
	}

	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8)) {
		fprintf(stderr, "[loadPNG] file %s is not recognized as a PNG file", filename.c_str());
		return 0;
	}


	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) {
		fprintf(stderr, "[loadPNG] png_create_read_struct failed!");
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fprintf(stderr, "[loadPNG] png_create_info_struct failed!");
		return 0;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
		fprintf(stderr, "[loadPNG] png_jmpbuf failed!");

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	int bytes_per_row = png_get_rowbytes(png_ptr, info_ptr);

	img_info_t img_info;
	img_info.width = png_get_image_width(png_ptr, info_ptr);
	img_info.height = png_get_image_height(png_ptr, info_ptr);
	img_info.bpp = png_get_bit_depth(png_ptr, info_ptr);
	img_info.num_channels = png_get_channels(png_ptr, info_ptr);

	long total_bytes = img_info.height * bytes_per_row;

	//int color_type = png_get_color_type(png_ptr, info_ptr);

	//        int number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	png_byte *pixel_buffer = (png_byte*)malloc(sizeof(png_byte)*(total_bytes));
	*out_buffer = pixel_buffer; 

	if (setjmp(png_jmpbuf(png_ptr)))
		fprintf(stderr, "[loadPNG] png_jmpbuf failed!");

	if (!pixel_buffer) { 
		fprintf(stderr, "[loagPNG] main buffer malloc failed!\n"); 
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		fclose(fp);
		return 0; 
	}

	png_byte **row_pointers = (png_byte**)malloc(sizeof(png_byte*) * img_info.height);

	// assign row pointers individually as offsets into the contiguous opengl-compatible uchar buffer
	for (int i = 0; i < img_info.height; ++i) row_pointers[(img_info.height-1)-i] = pixel_buffer + i*bytes_per_row;        

	png_read_image(png_ptr, row_pointers);

	*out_header = img_info;

	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
	fclose(fp);

	if (out_header->total_bytes() != total_bytes) { fprintf(stderr, "WARNING: loadPNG: total_bytes() != total_bytes!! (%lld != %lld) \n", 
			(long long)out_header->total_bytes(), (long long)total_bytes); }


	return 1;

}

