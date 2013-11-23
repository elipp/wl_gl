#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>

#include "texturewl.h"
#include "jpeglib.h"

static std::string get_file_extension(const std::string &filename) {
	int i = 0;
	size_t fn_len = filename.length();
	while (i < fn_len && filename[i] != '.') {
		++i;
	}
	std::string ext = filename.substr(i+1, filename.length() - (i+1));
	//PRINT("get_file_extension: \"%s\"\n", ext.c_str());
	return ext;
}

int loadJPEG(const std::string &filename, unsigned char **out_buffer, unified_header_data *out_header, int *total_bytes) {

  struct jpeg_decompress_struct cinfo;

  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE* infile;		/* source file */
  JSAMPROW *buffer;
  int row_stride;		/* physical row width in output buffer */
  int err;

  infile = fopen(filename.c_str(), "rb");
  if (!infile) {
    fprintf(stderr, "loadJPEG: fatal: can't open %s\n", filename.c_str());
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  cinfo.err = jpeg_std_error(&jerr);
  /* Establish the setjmp return context for my_error_exit to use. */

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);

  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  *total_bytes = (cinfo.output_width*cinfo.output_components)*(cinfo.output_height*cinfo.output_components);
  *out_buffer = new unsigned char[*total_bytes];
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  unsigned char* outbuf_iter = *out_buffer;
  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    //put_scanline_someplace(buffer[0], row_stride);
	memcpy(outbuf_iter, buffer[0], row_stride);
	outbuf_iter += row_stride;
  }

  /* Step 7: Finish decompression */
  (void) jpeg_finish_decompress(&cinfo);

  
  out_header->width = cinfo.image_width;
  out_header->height = cinfo.image_height;
  out_header->bpp = cinfo.output_components * 8;	// safe assumption
 
  /* Step 8: Release JPEG decompression object */
  jpeg_destroy_decompress(&cinfo);

  fclose(infile);
  return 1;

}

int load_pixels(const std::string& filename, std::vector<unsigned char> &pixels, unified_header_data *img_info) {
	unsigned width = 0, height = 0;
	memset(img_info, 0x0, sizeof(*img_info));
	
	std::string ext = get_file_extension(filename);
	if (ext == "jpg" || ext == "jpeg") {
		unsigned char* pixel_buf;
		int total_bytes;
		if (!loadJPEG(filename, &pixel_buf, img_info, &total_bytes)) {
			fprintf(stderr, "load_pixels: fatal error: loading file %s failed.\n", filename.c_str());
			return 0;
		}
		else {
			pixels.assign(pixel_buf, pixel_buf + total_bytes);
			delete [] pixel_buf;
			return 1;
		}
	
	}
	else {
		fprintf(stderr, "load_pixels: fatal error: unsupported image file extension \"%s\" (only .jpg/.jpeg are supported)\n", ext.c_str());
		return 0;
	}
}


