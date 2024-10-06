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
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
	struct ClientState *state = (ClientState*)data;
	/* TODO */
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

wl_buffer* DrawFrame(ClientState *state)
{
	
	cint width = 1920, height = 1080;
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
	while (wl_display_dispatch(state.display)) {
		/* This space deliberately left blank */
	}
	
	return state.display;
}

}
