#pragma once

#include "../renderer.hpp"

class SoftwareRenderer : public Renderer {
public:
	SoftwareRenderer();
	~SoftwareRenderer();

	void Frame();
	void SetFrameBuffer(uint32_t frame_buffer_address, int frame_width, int pixel_format);
private:
	uint32_t frame_buffer_address = 0;
	int frame_width = 0;

	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
};