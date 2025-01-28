#include "renderer.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Renderer::Renderer() {
	window = SDL_CreateWindow("PSP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, BASE_WIDTH * 2, BASE_HEIGHT * 2, SDL_WINDOW_RESIZABLE);
	SDL_SetWindowMinimumSize(window, BASE_WIDTH, BASE_HEIGHT);
}

Renderer::~Renderer() {
	SDL_DestroyWindow(window);
}

void Renderer::Frame() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			PSP::GetInstance()->Exit();
			break;
		}
	}
	
}