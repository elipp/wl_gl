#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <streambuf>
#include <map>

#include "wl_stuff.h"
#include "lin_alg.h"
#include "vertex.h"
#include "timer.h"
#include "mesh.h"
#include "texturewl.h"

mat4 Projection;
mat4 View;

Quaternion viewq;

int WINDOW_WIDTH = 1680, WINDOW_HEIGHT = 1050;

GLuint ModelView_u;
GLuint Projection_u;
GLuint sampler0_u;

static GLuint grass_texId;

static vec4 camera_pos = vec4(0.0, 0.0, -15.0, 1.0);
static vec4 camera_vel = vec4(0.0, 0.0, 0.0, 0.0);
static mesh_t *terrain_mesh = NULL;
static mesh_t *car_mesh = NULL;

float mouse_dx = 0, mouse_dy = 0;
bool keys[256];


int running = 1;

inline size_t cpp_getfilesize(std::ifstream& in) {
	in.seekg (0, std::ios::end);
	long length = in.tellg();
	in.seekg(0, std::ios::beg);

	return length;
}

static GLuint create_shader(struct window *window, const char *filename, GLenum shader_type) {
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


int init_gl(struct window *window) {

	GLuint frag, vert;
	GLuint program;
	GLint status;

	Projection = mat4::proj_persp(M_PI/6, ((float)WINDOW_WIDTH/(float)WINDOW_HEIGHT), 4.0, 1000.0);
	glViewport(0, 0, window->geometry.width, window->geometry.height);

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
	
	if (!load_texture("textures/grass.jpg", &grass_texId, 0)) return 0;

//	if (!load_texture("textures/pngtest.png", &grass_texId, 0)) return 0;
	glUseProgram(program);

	glBindAttribLocation(program, ATTRIB_POSITION, "attrib_Position");
	glBindAttribLocation(program, ATTRIB_NORMAL, "attrib_Normal");
	glBindAttribLocation(program, ATTRIB_TEXCOORD, "attrib_TexCoord");
	
	ModelView_u = glGetUniformLocation(program, "ModelView");
	Projection_u = glGetUniformLocation(program, "Projection");
	sampler0_u = glGetUniformLocation(program, "texture0");

	terrain_mesh = new mesh_t("models/mappi.bobj", program, grass_texId);
	assert(terrain_mesh->is_bad() == false);

	car_mesh = new mesh_t("models/chassis.bobj", program, grass_texId);
	assert(car_mesh->is_bad() == false);

	GLushort *indices = new GLushort[0xFFFF];
	for (GLuint i = 0; i < 0xFFFF; ++i) {
		indices[i] = i;
	}

	/*glGenBuffers(1, &IBOid);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBOid);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0xFFFF*sizeof(GLushort), indices, GL_STATIC_DRAW);
	*/

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(true);

	return 1;
}

void redraw(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener frame_listener = {
	redraw
};

void rotateview(float modx, float mody) {
	static float qx = 0;
	const float qx_mod = 0.001;
	static float qy = 0;
	const float qy_mod = 0.001;
	qx -= qx_mod*modx;
	qy -= qy_mod*mody;
	Quaternion xq = Quaternion::fromAxisAngle(1.0, 0.0, 0.0, qy);
	Quaternion yq = Quaternion::fromAxisAngle(0.0, 1.0, 0.0, qx);
	viewq = yq*xq;
	viewq.normalize();
}

void update_c_pos() {
	camera_pos += camera_vel.applyQuatRotation(viewq);
	camera_pos.assign(V::w, 1.0);
	View = viewq.toRotationMatrix();
	View = View*mat4::translate(camera_pos);
}

void redraw(void *data, struct wl_callback *callback, uint32_t time) {

	struct window *window = (struct window*)data;

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

	if (keys[KEY_W]) camera_vel += vec4(0.0, 0.0, 0.01, 0.0);
	if (keys[KEY_S]) camera_vel -= vec4(0.0, 0.0, 0.01, 0.0);
	if (keys[KEY_A]) camera_vel += vec4(0.007, 0.0, 0.0, 0.0);
	if (keys[KEY_D]) camera_vel -= vec4(0.007, 0.0, 0.0, 0.0);

	#define TURNMOD 10
	// the pointer_lock thing hasn't been implemented or even adopted, so we'll do this in the meantime

	if (keys[KEY_UP]) mouse_dy = -TURNMOD;
	if (keys[KEY_DOWN]) mouse_dy = TURNMOD;
	if (keys[KEY_LEFT]) mouse_dx = -TURNMOD;
	if (keys[KEY_RIGHT]) mouse_dx = TURNMOD;

	camera_vel *= 0.975;
	rotateview(mouse_dx, mouse_dy);
	update_c_pos();

	mouse_dx = mouse_dy = 0;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 0.5);

	terrain_mesh->render();
	car_mesh->render();

	/*
	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0, window->geometry.width, window->geometry.height);

		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}*/

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);

	eglSwapBuffers(window->display->egl.dpy, window->egl_surface);

}

static void signal_int(int signum) {
	running = 0;
}

int main(int argc, char **argv) {

	struct sigaction sigint;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	int ret = 0;
	if (!create_gl_window(WINDOW_WIDTH, WINDOW_HEIGHT)) return 0;

	while (running && ret != -1)
		ret = wl_display_dispatch(display.display);

	destroy_gl_window();

	return 0;
}
