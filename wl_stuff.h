#ifndef WL_STUFF
#define WL_STUFF

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <linux/input.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

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

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct {
		GLuint ModelView_u;
		GLuint Projection_u;
		GLuint sampler0_u;
	} gl;

	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, configured, opaque;
};

void create_surface(struct window *window);
void destroy_surface(struct window *window);

extern const struct wl_registry_listener registry_listener;

#endif
