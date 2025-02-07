#include "renderer.hpp"

#include <spdlog/spdlog.h>

#include "../../psp.hpp"

SoftwareRenderer::SoftwareRenderer() : Renderer() {
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetLogicalSize(renderer, BASE_WIDTH, BASE_HEIGHT);
}

SoftwareRenderer::~SoftwareRenderer() {
	if (texture) SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
}

void SoftwareRenderer::Frame() {
	Renderer::Frame();

	if (texture && frame_buffer_address) {
		void* framebuffer = PSP::GetInstance()->VirtualToPhysical(frame_buffer_address);
		SDL_UpdateTexture(texture, nullptr, framebuffer, frame_width);
		SDL_RenderCopy(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
	}
	else if (!frame_buffer_address) {
		SDL_RenderClear(renderer);
	}

}

void SoftwareRenderer::SetFrameBuffer(uint32_t frame_buffer_address, int frame_width, int pixel_format) {
	if (texture) SDL_DestroyTexture(texture);

	uint32_t sdl_pixel_format = 0;
	switch (pixel_format) {
	case SCE_DISPLAY_PIXEL_RGB565: sdl_pixel_format = SDL_PIXELFORMAT_RGB565; break;
	case SCE_DISPLAY_PIXEL_RGBA5551: sdl_pixel_format = SDL_PIXELFORMAT_RGBA5551; break;
	case SCE_DISPLAY_PIXEL_RGBA4444: sdl_pixel_format = SDL_PIXELFORMAT_RGBA4444; break;
	case SCE_DISPLAY_PIXEL_RGBA8888: sdl_pixel_format = SDL_PIXELFORMAT_RGBA8888; frame_width *= 4; break;
	};

	if (sdl_pixel_format == 0) {
		spdlog::error("SoftwareRenderer: invalid pixel format");
	}

	texture = SDL_CreateTexture(renderer, sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, BASE_WIDTH, BASE_HEIGHT);

	this->frame_width = frame_width;
	this->frame_buffer_address = frame_buffer_address;
}