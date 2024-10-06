#pragma once

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

namespace cornus::wayland {

struct ClientState {
	// ...
	struct wl_compositor *compositor = 0;
	struct wl_shm *shm = 0;
	
	/* Globals */
	struct wl_display *display;
	struct wl_registry *registry;
	struct xdg_wm_base *xdg_wm_base;
	/* Objects */
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	
	float offset;
	uint32_t last_frame;
};


wl_buffer* DrawFrame(ClientState *state);
void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);
wl_display *test();


}
