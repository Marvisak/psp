#pragma once

#include <SDL2/SDL.h>

enum class RendererType {
	SOFTWARE
};

constexpr auto BASE_WIDTH = 480;
constexpr auto BASE_HEIGHT = 272;
constexpr auto REFRESH_RATE = 60;

class Renderer {
public:
	~Renderer();

	virtual void Frame();
	virtual void SetFrameBuffer(uint32_t frame_buffer_address, int frame_width, int pixel_format) = 0;
protected:
	Renderer();

	SDL_Window* window;
};