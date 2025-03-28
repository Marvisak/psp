#pragma once

#include <algorithm>

#include "../renderer.hpp"

struct DXT1Block {
	uint8_t lines[4];
	uint16_t color1;
	uint16_t color2;
};

struct DXT3Block {
	DXT1Block color;
	uint16_t alpha_lines[4];
};

struct DXT5Block {
	DXT1Block color;
	uint32_t alpha_data2;
	uint16_t alpha_data1;
	uint8_t alpha1;
	uint8_t alpha2;
};

class SoftwareRenderer : public Renderer {
public:
	SoftwareRenderer();
	~SoftwareRenderer();

	void Frame();
	void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format);
	void DrawRectangle(Vertex start, Vertex end);
	void DrawTriangle(Vertex v0, Vertex v1, Vertex v2);
	void DrawTriangleFan(std::vector<Vertex> vertices);

	void DecodeDXTColors(const DXT1Block* block, glm::ivec4 palette[4], bool skip_alpha);
	void WriteDXT1(const DXT1Block* block, glm::ivec4 palette[4], Color* dst, int pitch);
	void WriteDXT3(const DXT3Block* block, glm::ivec4 palette[4], Color* dst, int pitch);
	void WriteDXT5(const DXT5Block* block, glm::ivec4 palette[4], Color* dst, int pitch);

	bool Test(uint8_t test, int src, int dst);
	uint8_t GetFilter(float ds, float dt);

	Color Blend(Color src, Color dest);
	glm::uvec2 FilterTexture(uint8_t filter, glm::vec2 uv);
	Color GetCLUT(uint32_t index);
	std::vector<Color> DecodeTexture();
private:
	SDL_PixelFormat frame_format = SDL_PIXELFORMAT_UNKNOWN;
	uint32_t frame_buffer = 0;
	int frame_width = 0;

	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
};