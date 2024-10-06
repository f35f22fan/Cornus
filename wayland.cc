#include "wayland.hh"

#include "err.hpp"
#include "io/io.hh"
#include <stdio.h>
#include <sys/mman.h>

namespace cornus::wayland {

static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	//mtl_info("Releasing buffer");
	/* Sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static void
xdg_surface_configure(void *data,
	struct xdg_surface *xdg_surface, uint32_t serial)
{
	ClientState *state = (ClientState*)data;
	
	xdg_surface_ack_configure(xdg_surface, serial);
	
	struct wl_buffer *buffer = DrawFrame(state);
	wl_surface_attach(state->surface, buffer, 0, 0);
	wl_surface_commit(state->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);
	
	/* Request another frame */
	struct ClientState *state = (ClientState*)data;
	cb = wl_surface_frame(state->surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);
	
	/* Update scroll amount at 24 pixels per second */
	if (state->last_frame != 0) {
		int elapsed = time - state->last_frame;
		state->offset += elapsed / 1000.0 * 24;
	}
	
	/* Submit a frame for this event */
	struct wl_buffer *buffer = DrawFrame(state);
	wl_surface_attach(state->surface, buffer, 0, 0);
	wl_surface_damage_buffer(state->surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(state->surface);
	
	state->last_frame = time;
}

static void
wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
	uint32_t serial, struct wl_surface *surface,
	wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	struct ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.surface_x = surface_x,
	client_state->pointer_event.surface_y = surface_y;
}

static void
wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
	uint32_t serial, struct wl_surface *surface)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

static void
wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
	wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_MOTION;
	client_state->pointer_event.time = time;
	client_state->pointer_event.surface_x = surface_x,
	client_state->pointer_event.surface_y = surface_y;
}

static void
wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
	client_state->pointer_event.time = time;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.button = button,
	client_state->pointer_event.state = state;
}

static void
wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
	uint32_t axis, wl_fixed_t value)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS;
	client_state->pointer_event.time = time;
	client_state->pointer_event.axes[axis].valid = true;
	client_state->pointer_event.axes[axis].value = value;
}

static void
wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
	uint32_t axis_source)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
	client_state->pointer_event.axis_source = axis_source;
}

static void
wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
	uint32_t time, uint32_t axis)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.time = time;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_STOP;
	client_state->pointer_event.axes[axis].valid = true;
}

static void
wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
	uint32_t axis, int32_t discrete)
{
	ClientState *client_state = (ClientState*)data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
	client_state->pointer_event.axes[axis].valid = true;
	client_state->pointer_event.axes[axis].discrete = discrete;
}

static void
wl_pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
	ClientState *client_state = (ClientState*)data;
	struct pointer_event *event = &client_state->pointer_event;
	fprintf(stderr, "pointer frame @ %d: ", event->time);
	
	if (event->event_mask & POINTER_EVENT_ENTER) {
		fprintf(stderr, "entered %f, %f ",
			wl_fixed_to_double(event->surface_x),
			wl_fixed_to_double(event->surface_y));
	}
	
	if (event->event_mask & POINTER_EVENT_LEAVE) {
		fprintf(stderr, "leave");
	}
	
	if (event->event_mask & POINTER_EVENT_MOTION) {
		fprintf(stderr, "motion %f, %f ",
			wl_fixed_to_double(event->surface_x),
			wl_fixed_to_double(event->surface_y));
	}
	
	if (event->event_mask & POINTER_EVENT_BUTTON) {
		const char *state = event->state == WL_POINTER_BUTTON_STATE_RELEASED ?
						  "released" : "pressed";
		fprintf(stderr, "button %d %s ", event->button, state);
	}
	
	uint32_t axis_events = POINTER_EVENT_AXIS
						   | POINTER_EVENT_AXIS_SOURCE
						   | POINTER_EVENT_AXIS_STOP
						   | POINTER_EVENT_AXIS_DISCRETE;
	const char *axis_name[2] = {
		[WL_POINTER_AXIS_VERTICAL_SCROLL] = "vertical",
		[WL_POINTER_AXIS_HORIZONTAL_SCROLL] = "horizontal",
	};
	const char *axis_source[4] = {
		[WL_POINTER_AXIS_SOURCE_WHEEL] = "wheel",
		[WL_POINTER_AXIS_SOURCE_FINGER] = "finger",
		[WL_POINTER_AXIS_SOURCE_CONTINUOUS] = "continuous",
		[WL_POINTER_AXIS_SOURCE_WHEEL_TILT] = "wheel tilt",
	};
	if (event->event_mask & axis_events) {
		for (size_t i = 0; i < 2; ++i) {
			if (!event->axes[i].valid) {
				continue;
			}
			fprintf(stderr, "%s axis ", axis_name[i]);
			if (event->event_mask & POINTER_EVENT_AXIS) {
				fprintf(stderr, "value %f ", wl_fixed_to_double(
												 event->axes[i].value));
			}
			if (event->event_mask & POINTER_EVENT_AXIS_DISCRETE) {
				fprintf(stderr, "discrete %d ",
					event->axes[i].discrete);
			}
			if (event->event_mask & POINTER_EVENT_AXIS_SOURCE) {
				fprintf(stderr, "via %s ",
					axis_source[event->axis_source]);
			}
			if (event->event_mask & POINTER_EVENT_AXIS_STOP) {
				fprintf(stderr, "(stopped) ");
			}
		}
	}
	
	fprintf(stderr, "\n");
	memset(event, 0, sizeof(*event));
}

static const struct wl_pointer_listener wl_pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

static void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t format, int32_t fd, uint32_t size)
{
	struct ClientState *client_state = (ClientState*)data;
	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
	
	char *map_shm = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	assert(map_shm != MAP_FAILED);
	
	struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
		client_state->xkb_context, map_shm,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_shm, size);
	close(fd);
	
	struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
	xkb_keymap_unref(client_state->xkb_keymap);
	xkb_state_unref(client_state->xkb_state);
	client_state->xkb_keymap = xkb_keymap;
	client_state->xkb_state = xkb_state;
}

static void
wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, struct wl_surface *surface,
	struct wl_array *keys)
{
	struct ClientState *client_state = (ClientState*)data;
	fprintf(stderr, "keyboard enter; keys pressed are:\n");
	uint32_t *key;
	//wl_array_for_each(key, keys) {
	WL_ARRAY_FOR_EACH(key, keys, uint32_t*) {
		char buf[128];
		xkb_keysym_t sym = xkb_state_key_get_one_sym(
			client_state->xkb_state, *key + 8);
		xkb_keysym_get_name(sym, buf, sizeof(buf));
		fprintf(stderr, "sym: %-12s (%d), ", buf, sym);
		xkb_state_key_get_utf8(client_state->xkb_state,
			*key + 8, buf, sizeof(buf));
		fprintf(stderr, "utf8: '%s'\n", buf);
	}
}

static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	struct ClientState *client_state = (ClientState*)data;
	char buf[128];
	uint32_t keycode = key + 8;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(
		client_state->xkb_state, keycode);
	xkb_keysym_get_name(sym, buf, sizeof(buf));
	const char *action =
		state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release";
	fprintf(stderr, "key %s: sym: %-12s (%d), ", action, buf, sym);
	xkb_state_key_get_utf8(client_state->xkb_state, keycode,
		buf, sizeof(buf));
	fprintf(stderr, "utf8: '%s'\n", buf);
}

static void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, struct wl_surface *surface)
{
	fprintf(stderr, "keyboard leave\n");
}

static void
wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t mods_depressed,
	uint32_t mods_latched, uint32_t mods_locked,
	uint32_t group)
{
	struct ClientState *client_state = (ClientState*)data;
	xkb_state_update_mask(client_state->xkb_state,
		mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
	int32_t rate, int32_t delay)
{
	/* Left as an exercise for the reader */
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_modifiers,
	.repeat_info = wl_keyboard_repeat_info,
};

static void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
	struct ClientState *state = (ClientState*)data;
	cbool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
	
	if (have_pointer && (!state->wl_pointer)) {
		state->wl_pointer = wl_seat_get_pointer(state->wl_seat);
		wl_pointer_add_listener(state->wl_pointer,
			&wl_pointer_listener, state);
	} else if (!have_pointer && state->wl_pointer) {
		wl_pointer_release(state->wl_pointer);
		state->wl_pointer = 0;
	}
	
	cbool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
	if (have_keyboard && (!state->wl_keyboard)) {
		state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
		wl_keyboard_add_listener(state->wl_keyboard,
			&wl_keyboard_listener, state);
	} else if (!have_keyboard && state->wl_keyboard) {
		wl_keyboard_release(state->wl_keyboard);
		state->wl_keyboard = 0;
	}
}

static void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	struct ClientState *state = (ClientState*)data;
	fprintf(stderr, "seat name: %s\n", name);
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = wl_seat_name,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
	uint32_t name, const char *interface, uint32_t version)
{
	//mtl_info("Interface: %-30s version: %d, name: %d\n", interface, version, name);
	auto *state = (struct ClientState*)data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = (struct wl_compositor*)wl_registry_bind(
			registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = (struct wl_shm*) wl_registry_bind(
			registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		state->xdg_wm_base = (xdg_wm_base*)wl_registry_bind(
			registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(state->xdg_wm_base,
			&xdg_wm_base_listener, state);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = (wl_seat*)wl_registry_bind(
			registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat,
			&wl_seat_listener, state);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
	uint32_t name)
{
	// This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

void xdg_toplevel_configure(void *data,
	struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
	struct wl_array *states)
{
	struct ClientState *state = (struct ClientState*)data;
	if (width == 0 || height == 0) {
		// Compositor is deferring to us
		state->width = 640;
		state->height= 480;
		return;
	}
	state->width = width;
	state->height = height;
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct ClientState *state = (struct ClientState*)data;
	state->closed = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};


wl_buffer* DrawFrame(struct ClientState *state)
{
	
	cint width = state->width, height = state->height;
	cint stride = width * 4;
	cint pool_size = height * stride;
	cint fd = cornus::io::allocate_shm_file(pool_size);
	u32 *pool_data = (u32*)mmap(NULL, pool_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if (pool_data == MAP_FAILED) {
		close(fd);
		return 0;
	}
	
	struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, pool_size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
		width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	
	u32 *pixels = pool_data;
	// memset(pixels, 0xFF, stride * height); //white
	int offset = (int)state->offset % 8;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			cbool flag = ((x+offset) + (y+offset) / 8 * 8) % 16 < 8;
			pixels[y * width + x] = flag ? 0xFF666666 : 0xFFEEEEEE;
		}
	}
	munmap(pool_data, pool_size);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	
	return buffer;
}

wl_display* test()
{
	struct ClientState state = {};
	state.display = wl_display_connect(NULL);
	state.registry = wl_display_get_registry(state.display);
	state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	wl_registry_add_listener(state.registry, &registry_listener, &state);
	// Block until all pending requests are processed by the server
	wl_display_roundtrip(state.display);
	state.surface = wl_compositor_create_surface(state.compositor);
	state.xdg_surface = xdg_wm_base_get_xdg_surface(
		state.xdg_wm_base, state.surface);
	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	xdg_toplevel_set_title(state.xdg_toplevel, "Example client");
	wl_surface_commit(state.surface);
	struct wl_callback *cb = wl_surface_frame(state.surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);
	
	xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
	
	while (wl_display_dispatch(state.display)) {
		/* This space deliberately left blank */
	}
	
	return state.display;
}

}
