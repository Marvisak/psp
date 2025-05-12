#pragma once

#include <algorithm>
#include <unordered_map>

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

struct TextureCacheEntry {
	bool clut;
	int	unused_frames;
	uint32_t size;
	std::vector<Color> data;
};

class SoftwareRenderer : public Renderer {
public:
	SoftwareRenderer();
	~SoftwareRenderer();

	void Frame();
	void Resize(int width, int height) {}
	void RenderFramebufferChange() {}
	void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format);
	void DrawRectangle(Vertex start, Vertex end);
	void DrawTriangle(Vertex v0, Vertex v1, Vertex v2);
	void DrawTriangleStrip(std::vector<Vertex> vertices);
	void DrawTriangleFan(std::vector<Vertex> vertices);
	void ClearTextureCache();
	void ClearTextureCache(uint32_t addr, uint32_t size);

	void DecodeDXTColors(const DXT1Block* block, glm::ivec4 palette[4], bool skip_alpha);
	void WriteDXT1(const DXT1Block* block, glm::ivec4 palette[4], Color* dst, int pitch);
	void WriteDXT3(const DXT3Block* block, glm::ivec4 palette[4], Color* dst, int pitch);
	void WriteDXT5(const DXT5Block* block, glm::ivec4 palette[4], Color* dst, int pitch);

	bool Test(uint8_t test, int src, int dst);
	uint8_t GetFilter(float du, float dv);
	Color GetTexel(int x, int y, const std::vector<Color>& tex_data);

	Color Blend(Color src, Color dest);
	Color BlendTexture(Color texture, Color blending);
	Color FilterTexture(uint8_t filter, glm::vec2 uv, const std::vector<Color>& tex_data);
	Color GetCLUT(uint32_t index);
	TextureCacheEntry DecodeTexture();
private:
	SDL_PixelFormat frame_format = SDL_PIXELFORMAT_UNKNOWN;
	uint32_t frame_buffer = 0;
	int frame_width = 0;

	std::unordered_map<uint32_t, TextureCacheEntry> texture_cache{};

	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
};