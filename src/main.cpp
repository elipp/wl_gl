#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

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
#include "keyboard.h"
#include "shader.h"

#include "net/client.h"

mat4 Projection;
mat4 View;

Quaternion viewq;

int WINDOW_WIDTH = 800, WINDOW_HEIGHT = 600;

static GLuint grass_texId;

static vec4 camera_pos = vec4(0.0, 0.0, -15.0, 1.0);
static vec4 camera_vel = vec4(0.0, 0.0, 0.0, 0.0);

static ShaderProgram *terrain_shader, *chassis_shader;
static mesh_t *terrain_mesh, *chassis_mesh, *wheel_mesh;

float mouse_dx = 0, mouse_dy = 0;

int running = 1;

int init_gl(struct window *window) {

	Projection = mat4::proj_persp(M_PI/6, ((float)WINDOW_WIDTH/(float)WINDOW_HEIGHT), 4.0, 1000.0);
	glViewport(0, 0, window->geometry.width, window->geometry.height);

	if (!load_texture("textures/grass.jpg", &grass_texId, 0)) return 0;
	if (!load_texture("textures/dina_all.png", &text_texId, 0)) return 0;

	terrain_shader = new ShaderProgram("shaders/terrain", SHADER_ATTRIB_FORMAT_V3N3T2);
	chassis_shader = new ShaderProgram("shaders/chassis", SHADER_ATTRIB_FORMAT_V3N3T2);

	terrain_mesh = new mesh_t("models/mappi.bobj", terrain_shader, true, grass_texId);
	assert(terrain_mesh->is_bad() == false);

	chassis_mesh = new mesh_t("models/chassis.bobj", chassis_shader, false);
	assert(chassis_mesh->is_bad() == false);

	wheel_mesh = new mesh_t("models/wheel.bobj", chassis_shader, false);
	assert(wheel_mesh->is_bad() == false);

	text_shader = new ShaderProgram("shaders/text_shader", SHADER_ATTRIB_FORMAT_V2T2);

	onScreenLog::init();
	VarTracker::init();
	
	/*
	GLushort *indices = new GLushort[0xFFFF];
	for (GLuint i = 0; i < 0xFFFF; ++i) {
		indices[i] = i;
	}

	glGenBuffers(1, &IBOid);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBOid);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0xFFFF*sizeof(GLushort), indices, GL_STATIC_DRAW);
	*/

	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LEQUAL);
	//glDepthMask(true);

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

void draw_connected_clients() {

	auto peers = LocalClient::get_peers();

	for (auto &iter : peers) {
		const Car &car =  iter.second.car;

		vec4 car_pos = car.position();

		mat4 modelview = View * mat4::translate(car_pos + vec4(0.0, -0.8, 0.0, 0.0)) * mat4::rotate(-car.state.direction, 0.0, 1.0, 0.0);
		mat4 mv = modelview*mat4::rotate(car.state.susp_angle_roll, 1.0, 0.0, 0.0);

		chassis_shader->update_uniform("vec4_color", (const GLvoid*)colors[iter.second.info.color].rawData());

		chassis_mesh->use_modelview(mv);
		chassis_mesh->render();

		static const vec4 wheel_color(0.07, 0.07, 0.07, 1.0);
		chassis_shader->update_uniform("vec4_color", (const GLvoid*)wheel_color.rawData());

		// front wheels
		static const mat4 front_left_wheel_translation = mat4::translate(-2.2, -0.6, 0.9);
		mv = modelview;
		mv *= front_left_wheel_translation;
		mv *= mat4::rotate(M_PI - car.state.front_wheel_angle, 0.0, 1.0, 0.0);
		mv *= mat4::rotate(-car.state.wheel_rot, 0.0, 0.0, 1.0);

		wheel_mesh->use_modelview(mv);
		wheel_mesh->render();

		static const mat4 front_right_wheel_translation = mat4::translate(-2.2, -0.6, -0.9);
		mv = modelview;
		mv *= front_right_wheel_translation;
		mv *= mat4::rotate(-car.state.front_wheel_angle, 0.0, 1.0, 0.0);
		mv *= mat4::rotate(car.state.wheel_rot, 0.0, 0.0, 1.0);

		wheel_mesh->use_modelview(mv);
		wheel_mesh->render();
	
		// back wheels
		static const mat4 back_left_wheel_translation = mat4::translate(1.3, -0.6, 0.9);
		mv = modelview;
		mv *= back_left_wheel_translation;
		mv *= mat4::rotate(car.state.wheel_rot, 0.0, 0.0, 1.0);
		mv *= mat4::rotate(M_PI, 0.0, 1.0, 0.0);

		wheel_mesh->use_modelview(mv);
		wheel_mesh->render();
	
		static const mat4 back_right_wheel_translation = mat4::translate(1.3, -0.6, -0.9);

		mv = modelview;
		mv *= back_right_wheel_translation;
		mv *= mat4::rotate(car.state.wheel_rot, 0.0, 0.0, 1.0);
		
		wheel_mesh->use_modelview(mv);
		wheel_mesh->render();


	}


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

	if (keys[KEY_W]) camera_vel += vec4(0.0, 0.0, 0.01, 0.0);
	if (keys[KEY_S]) camera_vel -= vec4(0.0, 0.0, 0.01, 0.0);
	if (keys[KEY_A]) camera_vel += vec4(0.007, 0.0, 0.0, 0.0);
	if (keys[KEY_D]) camera_vel -= vec4(0.007, 0.0, 0.0, 0.0);
	

	#define TURNMOD 10
	// the wayland pointer_lock thing hasn't been adopted to mainline, so we'll do this in the meantime

/*	if (keys[KEY_UP]) mouse_dy = -TURNMOD;
	if (keys[KEY_DOWN]) mouse_dy = TURNMOD;
	if (keys[KEY_LEFT]) mouse_dx = -TURNMOD;
	if (keys[KEY_RIGHT]) mouse_dx = TURNMOD;
	*/

	camera_vel *= 0.975;
	rotateview(mouse_dx, mouse_dy);
	update_c_pos();

	mouse_dx = mouse_dy = 0;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 0.5);

	terrain_mesh->use_modelview(View*mat4::scale(50, 50, 50));
	terrain_mesh->render();

	if (LocalClient::is_connected()) {
		LocalClient::interpolate_positions();
		draw_connected_clients();
	}

	onScreenLog::print("moikkelis\n");
	onScreenLog::dispatch_print_queue();
	onScreenLog::draw();
	
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

static void signal_int(int signum) {
	if (LocalClient::is_connected()) {
		LocalClient::request_shutdown();
	}
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

	LocalClient::set_name(get_login_username());	
	if (!LocalClient::connect("127.0.0.1:50000")) {
		PRINT("connecting to localhost:50000 failed!\n");
	}

	while (running && ret != -1) {
		ret = wl_display_dispatch(display.display);
	}

	destroy_gl_window();
	LocalClient::quit();

	return 0;
}
