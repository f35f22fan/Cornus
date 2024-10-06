#pragma once

#include <assert.h>
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200112L
#include <xkbcommon/xkbcommon.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "types.hxx"

namespace cornus::wayland {

#define WL_ARRAY_FOR_EACH(pos, array, type) \
for (pos = (type)(array)->data; \
	(const char *) pos < ((const char *) (array)->data + (array)->size); \
	(pos)++)
	


enum pointer_event_mask {
	POINTER_EVENT_ENTER = 1 << 0,
	POINTER_EVENT_LEAVE = 1 << 1,
	POINTER_EVENT_MOTION = 1 << 2,
	POINTER_EVENT_BUTTON = 1 << 3,
	POINTER_EVENT_AXIS = 1 << 4,
	POINTER_EVENT_AXIS_SOURCE = 1 << 5,
	POINTER_EVENT_AXIS_STOP = 1 << 6,
	POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct pointer_event {
	uint32_t event_mask;
	wl_fixed_t surface_x, surface_y;
	uint32_t button, state;
	uint32_t time;
	uint32_t serial;
	struct {
		bool valid;
		wl_fixed_t value;
		int32_t discrete;
	} axes[2];
	uint32_t axis_source;
};

struct ClientState {
	struct wl_seat *wl_seat = 0;
	struct wl_compositor *compositor = 0;
	struct wl_shm *shm = 0;
	
	// Globals
	struct wl_display *display = 0;
	struct wl_registry *registry = 0;
	struct xdg_wm_base *xdg_wm_base = 0;
	
	// Objects
	struct wl_surface *surface = 0;
	struct xdg_surface *xdg_surface = 0;
	struct xdg_toplevel *xdg_toplevel = 0;
	struct wl_keyboard *wl_keyboard = 0;
	struct wl_pointer *wl_pointer = 0;
	struct wl_touch *wl_touch = 0;
	
	struct pointer_event pointer_event;
	struct xkb_state *xkb_state = 0;
	struct xkb_context *xkb_context = 0;
	struct xkb_keymap *xkb_keymap = 0;
	
	float offset = 0;
	u32 last_frame = 0;
	int width;
	int height;
	bool closed = false;
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities);
wl_buffer* DrawFrame(ClientState *state);
void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);
void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
		i32 width, i32 height, struct wl_array *states);
wl_display *test();


}
