/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <streambuf>
#include <map>

#include "lin_alg.h"
#include "vertex.h"
#include "lzma/mylzma.h"

#define BUFFER_OFFSET(offset) ((char*)NULL + (offset))

GLuint IBOid;
std::map<GLuint, unsigned short> VBOid_numfaces_map;

struct window;
struct seat;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *default_cursor;
	struct wl_surface *cursor_surface;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	struct window *window;
};

struct geometry {
	int width, height;
};

enum { ATTRIB_POSITION = 0, ATTRIB_NORMAL = 1, ATTRIB_TEXCOORD = 2 };

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct {
		GLuint ModelView_u;
		GLuint Projection_u;
	} gl;

	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, configured, opaque;
};

static int running = 1;

	static void
init_egl(struct display *display, int opaque)
{
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n;
	EGLBoolean ret;

	if (opaque)
		config_attribs[9] = 0;

	display->egl.dpy = eglGetDisplay(display->display);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			&display->egl.conf, 1, &n);
	assert(ret && n == 1);

	display->egl.ctx = eglCreateContext(display->egl.dpy,
			display->egl.conf,
			EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

}

	static void
stop_egl(struct display *display)
{
	eglTerminate(display->egl.dpy);
	eglReleaseThread();
}
inline size_t cpp_getfilesize(std::ifstream& in) {
	in.seekg (0, std::ios::end);
	long length = in.tellg();
	in.seekg(0, std::ios::beg);

	return length;
}


static GLuint create_shader(struct window *window, const char *filename, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	std::ifstream in(filename);
	if (!in.is_open()) {
		std::cerr << "Couldn't open shader file.\n";
		exit(1);
	}

	std::string file_contents;
	in.seekg(0, std::ios::end);
	file_contents.reserve(in.tellg());
	in.seekg(0, std::ios::beg);

	file_contents.assign((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));

	std::cout<< file_contents;

	const GLchar *t = file_contents.c_str();

	glShaderSource(shader, 1, &t, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "shader: file %s\n", filename);
		fprintf(stderr, "Error: compiling %s: %*s\n",
				shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
				len, log);
		exit(1);
	}

	return shader;
}


void loadlzmabobj(const std::string &filename) {

#define NUM_FACES_PER_VBO_MAX ((0xFFFF)/3)
	std::ifstream infile(filename.c_str(), std::ios::binary);

	if (!infile.is_open()) { 
		fprintf(stderr, "Model: failed loading file %s\n", filename.c_str()); 
		return -1;
	}

	fprintf(stderr, ".bobjloader: loading file %s\n", filename.c_str());

	char *decompressed;
	size_t decompressed_size;

	int res = LZMA_decode(filename, &decompressed, &decompressed_size);	// this also allocates the memory
	if (res != 0) {
		fprintf(stderr, "LZMA decoder: an error occurred. Aborting.\n"); 
		return -1;
	}


	char* iter = decompressed + 4;	// the first 4 bytes of a bobj file contain the letters "bobj". Or do they? :D

	// read vertex & face count.

	unsigned int vcount;
	memcpy(&vcount, iter, sizeof(vcount));

	int total_facecount = vcount / 3;
	int num_VBOs = total_facecount/NUM_FACES_PER_VBO_MAX + 1;

	unsigned num_excess_last = total_facecount%NUM_FACES_PER_VBO_MAX;

	fprintf(stderr, "# faces = %d => num_VBOs = %d, num_excess_last = %d\n", total_facecount, num_VBOs, num_excess_last);

	GLuint *VBOids = new GLuint[num_VBOs];
	glGenBuffers(num_VBOs, VBOids);
	unsigned i = 0;
	for (i = 0; i < num_VBOs-1; ++i) {
		VBOid_numfaces_map.insert(std::pair<GLuint, unsigned short>(VBOids[i], NUM_FACES_PER_VBO_MAX));
	}
	VBOid_numfaces_map.insert(std::pair<GLuint,unsigned short>(VBOids[i], num_excess_last));
	delete [] VBOids;

	vertex* vertices = new vertex[vcount];

	iter = decompressed + 8;

	memcpy(vertices, iter, vcount*8*sizeof(float));
	delete [] decompressed;	// not needed anymore

	size_t offset = 0;
	for (auto &iter : VBOid_numfaces_map) {
		const GLuint &current_VBOid = iter.first;
		const unsigned short &num_faces = iter.second;
		glBindBuffer(GL_ARRAY_BUFFER, current_VBOid);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex)*num_faces*3, &vertices[offset].vx, GL_STATIC_DRAW);
		offset += NUM_FACES_PER_VBO_MAX*3;
	}

	fprintf(stderr, "Successfully loaded file %s.\n\n", filename.c_str());
	delete [] vertices;
}


static void init_gl(struct window *window)
{
	GLuint frag, vert;
	GLuint program;
	GLint status;

	frag = create_shader(window, "shaders/fs", GL_FRAGMENT_SHADER);
	vert = create_shader(window, "shaders/vs", GL_VERTEX_SHADER);

	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	loadlzmabobj("models/terrain.bobj");

	glUseProgram(program);

	glEnableVertexAttribArray(ATTRIB_POSITION);
	glEnableVertexAttribArray(ATTRIB_NORMAL);
	glEnableVertexAttribArray(ATTRIB_TEXCOORD);


	glBindAttribLocation(program, ATTRIB_POSITION, "attrib_Position");
	glBindAttribLocation(program, ATTRIB_NORMAL, "attrib_Normal");
	glBindAttribLocation(program, ATTRIB_TEXCOORD, "attrib_TexCoord");
	glLinkProgram(program);

//	glEnable(GL_DEPTH_TEST);

	window->gl.ModelView_u = glGetUniformLocation(program, "ModelView");
	window->gl.Projection_u = glGetUniformLocation(program, "Projection");

	GLushort *indices = new GLushort[0xFFFF];
	for (GLuint i = 0; i < 0xFFFF; ++i) {
		indices[i] = i;
	}

	glGenBuffers(1, &IBOid);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBOid);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0xFFFF*sizeof(GLushort), indices, GL_STATIC_DRAW);

}

	static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
		uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

	static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		uint32_t edges, int32_t width, int32_t height)
{
	struct window *window = (struct window*)data;

	if (window->native)
		wl_egl_window_resize(window->native, width, height, 0, 0);

	window->geometry.width = width;
	window->geometry.height = height;

	if (!window->fullscreen)
		window->window_size = window->geometry;
}

	static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static void configure_callback(void *data, struct wl_callback *callback, uint32_t  time) {
	struct window *window = (struct window*)data;
	wl_callback_destroy(callback);
	window->configured = 1;
	if (window->callback == NULL) {
		redraw(data, NULL, time);
	}
}

static struct wl_callback_listener configure_callback_listener = {
	configure_callback,
};

	static void
toggle_fullscreen(struct window *window, int fullscreen)
{
	struct wl_callback *callback;

	window->fullscreen = fullscreen;
	window->configured = 0;

	if (fullscreen) {
		wl_shell_surface_set_fullscreen(window->shell_surface,
				WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
				0, NULL);
	} else {
		wl_shell_surface_set_toplevel(window->shell_surface);
		handle_configure(window, window->shell_surface, 0,
				window->window_size.width,
				window->window_size.height);
	}

	callback = wl_display_sync(window->display->display);
	wl_callback_add_listener(callback, &configure_callback_listener,
			window);
}

	static void
create_surface(struct window *window)
{
	struct display *display = window->display;
	EGLBoolean ret;

	window->surface = wl_compositor_create_surface(display->compositor);
	window->shell_surface = wl_shell_get_shell_surface(display->shell,
			window->surface);

	wl_shell_surface_add_listener(window->shell_surface,
			&shell_surface_listener, window);

	window->native =
		wl_egl_window_create(window->surface,
				window->window_size.width,
				window->window_size.height);
	window->egl_surface =
		eglCreateWindowSurface(display->egl.dpy,
				display->egl.conf,
				window->native, NULL);

	wl_shell_surface_set_title(window->shell_surface, "simple-egl");

	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	toggle_fullscreen(window, window->fullscreen);
}

	static void
destroy_surface(struct window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
			EGL_NO_CONTEXT);

	eglDestroySurface(window->display->egl.dpy, window->egl_surface);
	wl_egl_window_destroy(window->native);

	wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);

	if (window->callback)
		wl_callback_destroy(window->callback);
}


static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
	redraw
};


static void redraw(void *data, struct wl_callback *callback, uint32_t time) {

	struct window *window = (struct window*)data;

	static const int32_t speed_div = 5;
	static uint32_t start_time = 0;
	struct wl_region *region;

	assert(window->callback == callback);
	window->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (!window->configured)
		return;

	if (start_time == 0)
		start_time = time;

	static float running = 0;
	running += 0.01;

	mat4 Projection = mat4::proj_persp(M_PI/6, (window->geometry.width/window->geometry.height), 4.0, 1000.0);
	mat4 ModelView = mat4::translate(0.0, 0.0, -15.0)*mat4::rotate(running, 0.0, 1.0, 0.0);

	glViewport(0, 0, window->geometry.width, window->geometry.height);

	glUniformMatrix4fv(window->gl.ModelView_u, 1, GL_FALSE,
			(const GLfloat *)ModelView.rawData());
	glUniformMatrix4fv(window->gl.Projection_u, 1, GL_FALSE,
			(const GLfloat*)Projection.rawData());

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 0.5);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBOid);

	for (auto &iter : VBOid_numfaces_map) {


		const GLuint &current_VBOid = iter.first;
		const unsigned short &num_faces = iter.second;

		glBindBuffer(GL_ARRAY_BUFFER, current_VBOid);   
		glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(0));
		glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(3*sizeof(float)));
		glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), BUFFER_OFFSET(6*sizeof(float)));

		glDrawElements(GL_TRIANGLES, num_faces*3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
	}

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0, window->geometry.width, window->geometry.height);

		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);

	eglSwapBuffers(window->display->egl.dpy, window->egl_surface);
}

	static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *display = (struct display*)data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = display->default_cursor;
	struct wl_cursor_image *image;

	if (display->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = display->default_cursor->images[0];
		buffer = wl_cursor_image_get_buffer(image);
		wl_pointer_set_cursor(pointer, serial, display->cursor_surface, image->hotspot_x, image->hotspot_y);
		wl_surface_attach(display->cursor_surface, buffer, 0, 0);
		wl_surface_damage(display->cursor_surface, 0, 0,
				image->width, image->height);
		wl_surface_commit(display->cursor_surface);
	}
}

	static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface)
{
}

	static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

	static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state)
{
	struct display *display = (struct display*)data;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
		wl_shell_surface_move(display->window->shell_surface,
				display->seat, serial);
}

	static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

	static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		uint32_t format, int fd, uint32_t size)
{
}

	static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, struct wl_surface *surface,
		struct wl_array *keys)
{
}

	static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, struct wl_surface *surface)
{
}

	static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, uint32_t time, uint32_t key,
		uint32_t state)
{
	struct display *d = (struct display*) data;

	if (key == KEY_F11 && state) {
		toggle_fullscreen(d->window, d->window->fullscreen ^ 1);

	}
	else if (key == KEY_ESC && state) {
		running = 0;
	}
}

	static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, uint32_t mods_depressed,
		uint32_t mods_latched, uint32_t mods_locked,
		uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

	static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
		enum wl_seat_capability caps)
{
	struct display *d = (struct display*)data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
		d->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(d->pointer, &pointer_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
		wl_pointer_destroy(d->pointer);
		d->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities
};

	static void 
registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	struct display *d = (display*)data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor = (struct wl_compositor*)wl_registry_bind(registry, name, (const wl_interface*)&wl_compositor_interface, 1);

	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = (struct wl_shell*)wl_registry_bind(registry, name, (const wl_interface*)&wl_shell_interface, 1);

	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = (struct wl_seat*)wl_registry_bind(registry, name, (const wl_interface*)&wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);

	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = (struct wl_shm*)wl_registry_bind(registry, name, (const wl_interface*)&wl_shm_interface, 1);
		d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
		d->default_cursor = wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
	}
}


	static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

	static void
signal_int(int signum)
{
	running = 0;
}

	static void
usage(int error_code)
{
	fprintf(stderr, "Usage: simple-egl [OPTIONS]\n\n"
			"  -f\tRun in fullscreen mode\n"
			"  -o\tCreate an opaque surface\n"
			"  -h\tThis help text\n\n");

	exit(error_code);
}

	int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	struct window  window  = { 0 };
	int i, ret = 0;

	window.display = &display;
	display.window = &window;
	window.window_size.width  = 640;
	window.window_size.height = 480;

	window.opaque = 1;
	for (i = 1; i < argc; i++) {
		if (strcmp("-f", argv[i]) == 0)
			window.fullscreen = 1;
		else if (strcmp("-h", argv[i]) == 0)
			usage(EXIT_SUCCESS);
		else
			usage(EXIT_FAILURE);
	}

	display.display = wl_display_connect(NULL);
	assert(display.display);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry,
			&registry_listener, &display);

	wl_display_dispatch(display.display);

	init_egl(&display, window.opaque);
	create_surface(&window);
	init_gl(&window);

	display.cursor_surface =
		wl_compositor_create_surface(display.compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	while (running && ret != -1)
		ret = wl_display_dispatch(display.display);

	fprintf(stderr, "simple-egl exiting\n");

	destroy_surface(&window);
	stop_egl(&display);

	wl_surface_destroy(display.cursor_surface);
	if (display.cursor_theme)
		wl_cursor_theme_destroy(display.cursor_theme);

	if (display.shell)
		wl_shell_destroy(display.shell);

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	wl_registry_destroy(display.registry);
	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	return 0;
}
