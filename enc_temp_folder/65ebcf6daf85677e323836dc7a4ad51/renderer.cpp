#include "renderer.hpp"

#include <spdlog/spdlog.h>
#include <glm/packing.hpp>

#include "../../psp.hpp"

SoftwareRenderer::SoftwareRenderer() : Renderer() {
	renderer = SDL_CreateRenderer(window, NULL);
	SDL_SetRenderLogicalPresentation(renderer, BASE_WIDTH, BASE_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

SoftwareRenderer::~SoftwareRenderer() {
	if (texture) SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
}

void SoftwareRenderer::Frame() {
	Renderer::Frame();

	SDL_RenderClear(renderer);
	if (texture && frame_buffer) {
		void* framebuffer = PSP::GetInstance()->VirtualToPhysical(frame_buffer);
		SDL_UpdateTexture(texture, nullptr, framebuffer, frame_width);
		SDL_RenderTexture(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
	}
}

void SoftwareRenderer::SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format) {
	SDL_PixelFormat sdl_pixel_format{};
	switch (pixel_format) {
	case SCE_DISPLAY_PIXEL_RGB565: sdl_pixel_format = SDL_PIXELFORMAT_BGR565; frame_width *= 2; break;
	case SCE_DISPLAY_PIXEL_RGBA5551: sdl_pixel_format = SDL_PIXELFORMAT_ABGR1555; frame_width *= 2; break;
	case SCE_DISPLAY_PIXEL_RGBA4444: sdl_pixel_format = SDL_PIXELFORMAT_ABGR4444; frame_width *= 2; break;
	case SCE_DISPLAY_PIXEL_RGBA8888: sdl_pixel_format = SDL_PIXELFORMAT_ABGR8888; frame_width *= 4; break;
	};

	this->frame_width = frame_width;
	this->frame_buffer = frame_buffer;

	if (this->frame_format == sdl_pixel_format) {
		return;
	}
	this->frame_format = sdl_pixel_format;

	if (texture) {
		SDL_DestroyTexture(texture);
	}

	if (sdl_pixel_format == SDL_PIXELFORMAT_UNKNOWN) {
		spdlog::error("SoftwareRenderer: invalid pixel format");
	}

	texture = SDL_CreateTexture(renderer, sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, BASE_WIDTH, BASE_HEIGHT);
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
}

void SoftwareRenderer::DrawRectangle(Vertex start, Vertex end) {
	uint32_t* frame_buffer_start = reinterpret_cast<uint32_t*>(PSP::GetInstance()->VirtualToPhysical(GetFrameBufferAddress()));
	uint16_t* z_buffer_start = reinterpret_cast<uint16_t*>(PSP::GetInstance()->VirtualToPhysical(GetZBufferAddress()));
	
	glm::ivec3 min = glm::round(start.pos);
	glm::ivec3 max = glm::round(end.pos);

	if (min.x > max.x) {
		std::swap(start.pos.x, end.pos.x);
		std::swap(min.x, max.x);
		std::swap(start.uv.x, end.uv.x);
	}

	if (min.y > max.y) {
		std::swap(start.pos.y, end.pos.y);
		std::swap(min.y, max.y);
		std::swap(start.uv.y, end.uv.y);
	}

	uint32_t width = max.x - min.x;
	uint32_t height = max.y - min.y;
	if (width == 0 || height == 0) return;

	if (fpf != SCE_DISPLAY_PIXEL_RGBA8888) {
		spdlog::error("SoftwareRenderer: unimplemented pixel format for triangle {}", fpf);
		return;
	}

	bool use_texture = !clear_mode && textures_enabled;

	uint8_t filter = -1;
	std::vector<Color> texture{};
	if (use_texture) {
		texture = DecodeTexture();
		filter = GetFilter(static_cast<float>(textures[0].width) / width, static_cast<float>(textures[0].height) / height);
	}

	int scissor_min_x = ScissorTestX(min.x);
	int scissor_max_x = ScissorTestX(max.x);
	int scissor_min_y = ScissorTestY(min.y);
	int scissor_max_y = ScissorTestY(max.y);

	float inv_min_z = 1.0f / start.pos.z;
	float inv_max_z = 1.0f / start.pos.z;

	float du = (end.uv.x - start.uv.x) / width;
	float dv = (end.uv.y - start.uv.y) / height;
	for (int y = scissor_min_y; y < scissor_max_y; y++) {
		glm::vec2 uv{
			start.uv.x,
			start.uv.y + (y - min.y) * dv
		};

		uint32_t frame_buffer_index = y * fbw;
		uint32_t z_buffer_index = y * zbw;
		for (int x = scissor_min_x; x < scissor_max_x; x++) {
			if (!through && (max.z < min_z || max.z > max_z)) {
				return;
			}

			uv.x += du;
			if (!clear_mode && depth_test) {
				if (!Test(z_test_func, max.z, z_buffer_start[z_buffer_index + x])) {
					continue;
				}
			}

			Color color = end.color;
			if (use_texture) {
				glm::uvec2 tex_pos = FilterTexture(filter, uv);
				Color texel = texture[tex_pos.y * textures[0].width + tex_pos.x];
				color = BlendTexture(texel, color);
			} 

			if (!clear_mode && alpha_test) {
				if (!Test(alpha_test_func, color.a & alpha_test_mask, alpha_test_ref & alpha_test_mask)) {
					continue;
				}
			}

			auto dest = static_cast<Color>(frame_buffer_start[frame_buffer_index + x]);
			if (!clear_mode && blend) {
				color = Blend(color, dest);
			}
			color = (dest.a << 24) | (color.abgr & 0xFFFFFF);

			frame_buffer_start[frame_buffer_index + x] = color.abgr;
			if (depth_write && depth_test) {
				z_buffer_start[z_buffer_index + x] = max.z;
			}
		}
	}
}

void SoftwareRenderer::DrawTriangle(Vertex v0, Vertex v1, Vertex v2) {
	auto edge = [](glm::ivec3 v0, glm::ivec3 v1, glm::ivec3 v2) -> int {
		return (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
		};

	auto topleft = [](glm::ivec3 v0, glm::ivec3 v1) -> bool {
		return (v1.y == v0.y && v1.x > v0.x) || (v1.y > v0.y);
	};

	uint32_t* frame_buffer_start = reinterpret_cast<uint32_t*>(PSP::GetInstance()->VirtualToPhysical(GetFrameBufferAddress()));
	uint16_t* z_buffer_start = reinterpret_cast<uint16_t*>(PSP::GetInstance()->VirtualToPhysical(GetZBufferAddress()));

	int min_x = floorf(std::min({ v0.pos.x, v1.pos.x, v2.pos.x }));
	int max_x = ceilf(std::max({ v0.pos.x, v1.pos.x, v2.pos.x }));
	int min_y = floorf(std::min({ v0.pos.y, v1.pos.y, v2.pos.y }));
	int max_y = ceilf(std::max({ v0.pos.y, v1.pos.y, v2.pos.y }));

	int area = edge(v0.pos, v1.pos, v2.pos);
	if (area == 0) return;

	/*if (culling) {
		switch (cull_type) {
		case SCEGU_CW:
			if (area < 0) {
				return;
			}
			break;
		case SCEGU_CCW:
			if (area > 0) {
				return;
			}
			std::swap(v0, v1);
			area = edge(v0.pos, v1.pos, v2.pos);
			break;
		}
	}
	else*/ if (area < 0) {
		std::swap(v0, v1);
		area = edge(v0.pos, v1.pos, v2.pos);
	}

	if (fpf != SCE_DISPLAY_PIXEL_RGBA8888) {
		spdlog::error("SoftwareRenderer: unimplemented pixel format for triangle {}", fpf);
		return;
	}

	bool use_texture = !clear_mode && textures_enabled;
	std::vector<Color> texture{};
	if (use_texture) {
		texture = DecodeTexture();
	}

	int scissor_min_x = ScissorTestX(min_x);
	int scissor_max_x = ScissorTestX(max_x);
	int scissor_min_y = ScissorTestY(min_y);
	int scissor_max_y = ScissorTestY(max_y);

	bool topleft1 = topleft(v0.pos, v1.pos);
	bool topleft2 = topleft(v1.pos, v2.pos);
	bool topleft3 = topleft(v2.pos, v0.pos);

	for (int y = scissor_min_y; y < scissor_max_y; y++) {
		for (int x = scissor_min_x; x < scissor_max_x; x++) {
			glm::ivec3 p{ x, y, 0.0 };

			int e1 = edge(v0.pos, v1.pos, p);
			int e2 = edge(v1.pos, v2.pos, p);
			int e3 = edge(v2.pos, v0.pos, p);

			if ((e1 > 0 || (e1 == 0 && topleft1)) &&
				(e2 > 0 || (e2 == 0 && topleft2)) &&
				(e3 > 0 || (e3 == 0 && topleft3))) {

				float w1 = static_cast<float>(e1) / area;
				float w2 = static_cast<float>(e2) / area;
				float w3 = static_cast<float>(e3) / area;

				uint16_t z = v2.pos.z * w1 + v0.pos.z * w2 + v1.pos.z * w3;
				if (!through && (z < min_z || z > max_z)) {
					continue;
				}

				if (!clear_mode && depth_test) {
					if (!Test(z_test_func, z, z_buffer_start[x + y * zbw])) {
						continue;
					}
				}

				Color color = v0.color;
				if (use_texture) {
					glm::vec2 uv{
						v2.uv.x * w1 + v0.uv.x * w2 + v1.uv.x * w3,
						v2.uv.y * w1 + v0.uv.y * w2 + v1.uv.y * w3,
					};
					glm::uvec2 tex_pos = FilterTexture(true, uv);
					Color texel = texture[tex_pos.y * textures[0].width + tex_pos.x];
					color = BlendTexture(texel, color);
				} else if (gouraud_shading) {
					color.r = v2.color.r * w1 + v0.color.r * w2 + v1.color.r * w3;
					color.g = v2.color.g * w1 + v0.color.g * w2 + v1.color.g * w3;
					color.b = v2.color.b * w1 + v0.color.b * w2 + v1.color.b * w3;
				}

				if (!clear_mode && alpha_test) {
					if (!Test(alpha_test_func, color.a & alpha_test_mask, alpha_test_ref & alpha_test_mask)) {
						continue;
					}
				}

				if (!clear_mode && blend) {
					color = Blend(color, static_cast<Color>(frame_buffer_start[x + y * fbw]));
				}

				frame_buffer_start[x + y * fbw] = color.abgr;
				if (depth_write) {
					z_buffer_start[x + y * zbw] = z;
				}
			}
		}
	}
}

void SoftwareRenderer::DrawTriangleFan(std::vector<Vertex> vertices) {
	for (int i = 1; i < vertices.size() - 1; i++) {
		DrawTriangle(vertices[0], vertices[i], vertices[i + 1]);
	}
}

void SoftwareRenderer::DecodeDXTColors(const DXT1Block* block, glm::ivec4 palette[4], bool skip_alpha) {
	palette[0] = {
		(block->color1 >> 8) & 0xF8,
		(block->color1 >> 3) & 0xFC,
		(block->color1 << 3) & 0xF8,
		skip_alpha ? 0 : 0xFF
	};

	palette[1] = {
		(block->color2 >> 8) & 0xF8,
		(block->color2 >> 3) & 0xFC,
		(block->color2 << 3) & 0xF8,
		skip_alpha ? 0 : 0xFF
	};

	if (block->color1 > block->color2) {
		palette[2] = (2 * palette[0] + palette[1]) / 3;
		palette[3] = (palette[0] + 2 * palette[1]) / 3;
	}
	else {
		palette[2] = (palette[0] + palette[1]) / 2;
		palette[3] = glm::ivec4(0);
	}
}

void SoftwareRenderer::WriteDXT1(const DXT1Block* block, glm::ivec4 palette[4], Color* dst, int pitch) {
	for (int y = 0; y < 4; y++) {
		int color_data = block->lines[y];
		for (int x = 0; x < 4; x++) {
			int col = color_data & 3;
			auto& current_palette = palette[col];

			dst[x] = {
				static_cast<uint8_t>(std::clamp(current_palette.a, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.b, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.g, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.r, 0, 255))
			};
			color_data >>= 2;
		}
		dst += pitch;
	}
}

void SoftwareRenderer::WriteDXT3(const DXT3Block* block, glm::ivec4 palette[4], Color* dst, int pitch) {
	for (int y = 0; y < 4; y++) {
		int color_data = block->color.lines[y];
		int alpha_data = block->alpha_lines[y];
		for (int x = 0; x < 4; x++) {
			int col = color_data & 3;
			auto& current_palette = palette[col];

			dst[x] = {
				static_cast<uint8_t>(std::clamp((alpha_data << 4) & 0xFF, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.b, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.g, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.r, 0, 255))
			};
			color_data >>= 2;
			alpha_data >>= 4;
		}
		dst += pitch;
	}
}

void SoftwareRenderer::WriteDXT5(const DXT5Block* block, glm::ivec4 palette[4], Color* dst, int pitch) {
	uint8_t alpha_table[8]{};

	alpha_table[0] = block->alpha1;
	alpha_table[1] = block->alpha2;
	if (block->alpha1 > block->alpha2) {
		for (int i = 2; i < 8; i++) {
			int alpha1 = (block->alpha1 * ((7 - (i - 1)) << 8)) / 7;
			int alpha2 = (block->alpha2 * ((i - 1) << 8)) / 7;
			alpha_table[i] = (alpha1 + alpha2 + 31) >> 8;
		}
	}
	else {
		for (int i = 2; i < 6; i++) {
			int alpha1 = (block->alpha1 * ((5 - (i - 1)) << 8)) / 5;
			int alpha2 = (block->alpha2 * ((i - 1) << 8)) / 5;
			alpha_table[i] = (alpha1 + alpha2 + 31) >> 8;
		}
		alpha_table[6] = 0;
		alpha_table[7] = 255;
	}

	uint64_t all_alpha = (static_cast<uint64_t>(block->alpha_data1) << 32) | block->alpha_data2;
	for (int y = 0; y < 4; y++) {
		int color_data = block->color.lines[y];
		int alpha_data = all_alpha >> (12 * y);
		for (int x = 0; x < 4; x++) {
			int col = color_data & 3;
			auto& current_palette = palette[col];

			dst[x] = {
				static_cast<uint8_t>(std::clamp(static_cast<int>(alpha_table[alpha_data & 7]), 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.b, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.g, 0, 255)),
				static_cast<uint8_t>(std::clamp(current_palette.r, 0, 255))
			};
			color_data >>= 2;
			alpha_data >>= 4;
		}
		dst += pitch;
	}
}

bool SoftwareRenderer::Test(uint8_t test, int src, int dst) {
	switch (test) {
	case SCEGU_NEVER:
		return false;
	case SCEGU_ALWAYS:
		return true;
	case SCEGU_EQUAL:
		return src == dst;
	case SCEGU_NOTEQUAL:
		return src != dst;
	case SCEGU_LESS:
		return src < dst;
	case SCEGU_LEQUAL:
		return src <= dst;
	case SCEGU_GREATER:
		return src > dst;
	case SCEGU_GEQUAL:
		return src >= dst;
	}
	return true;
}

uint8_t SoftwareRenderer::GetFilter(float ds, float dt) {
	int detail{};

	int height = 1 << (textures[0].height & 0xF);
	int width = 1 << (textures[0].width & 0xF);
	switch (texture_level_mode) {
	case SCEGU_LOD_AUTO:
		detail = std::log2(std::max(std::abs(ds * width), std::abs(dt * height)));
		break;
	}
	detail += texture_level_offset;

	return detail > 0 ? texture_minify_filter : texture_magnify_filter;
}

Color SoftwareRenderer::GetCLUT(uint32_t index) {
	index = ((index >> clut_shift) & clut_mask) | (clut_offset & (clut_format == SCEGU_PF8888 ? 0xFF : 0x1FF));
	switch (clut_format) {
	case SCEGU_PF4444: {
		return ABGR4444ToABGR8888(reinterpret_cast<uint16_t*>(clut.data())[index]);
	}
	case SCEGU_PF8888: {
		return reinterpret_cast<Color*>(clut.data())[index];
	}
	default:
		spdlog::error("SoftwareRenderer: unknown clut format {}", clut_format);
	}
	return {};
}

Color SoftwareRenderer::Blend(Color src, Color dest) {
	glm::ivec4 src_color{ src.r, src.g, src.b, src.a };
	glm::ivec4 dest_color{ dest.r, dest.g, dest.b, dest.a };

	glm::ivec4 asel{};
	glm::ivec4 bsel{};
	if (blend_operation < SCEGU_MIN) {
		switch (blend_source) {
		case SCEGU_COLOR: asel = dest_color; break;
		case SCEGU_ONE_MINUS_COLOR: asel = 255 - glm::ivec4(dest_color); break;
		case SCEGU_SRC_ALPHA: asel = glm::ivec4(src_color.a); break;
		case SCEGU_ONE_MINUS_SRC_ALPHA: asel = glm::ivec4(255 - src_color.a); break;
		case SCEGU_DST_ALPHA: asel = glm::ivec4(dest_color.a); break;
		case SCEGU_ONE_MINUS_DST_ALPHA: asel = glm::ivec4(255 - dest_color.a); break;
		case SCEGU_DOUBLE_SRC_ALPHA: asel = glm::ivec4(2 * src_color.a); break;
		case SCEGU_ONE_MINUS_DOUBLE_SRC_ALPHA: asel = glm::ivec4(255 - std::min(2 * src_color.a, 255)); break;
		case SCEGU_DOUBLE_DST_ALPHA: asel = glm::ivec4(2 * dest_color.a); break;
		case SCEGU_ONE_MINUS_DOUBLE_DST_ALPHA: asel = glm::ivec4(255 - std::min(2 * dest_color.a, 255)); break;
		default: asel = blend_afix; break;
		}

		switch (blend_destination) {
		case SCEGU_COLOR: bsel = src_color; break;
		case SCEGU_ONE_MINUS_COLOR: bsel = 255 - glm::ivec4(src_color); break;
		case SCEGU_SRC_ALPHA: bsel = glm::ivec4(src_color.a); break;
		case SCEGU_ONE_MINUS_SRC_ALPHA: bsel = glm::ivec4(255 - src_color.a); break;
		case SCEGU_DST_ALPHA: bsel = glm::ivec4(dest_color.a); break;
		case SCEGU_ONE_MINUS_DST_ALPHA: bsel = glm::ivec4(255 - dest_color.a); break;
		case SCEGU_DOUBLE_SRC_ALPHA: bsel = glm::ivec4(2 * src_color.a); break;
		case SCEGU_ONE_MINUS_DOUBLE_SRC_ALPHA: bsel = glm::ivec4(255 - std::min(2 * src_color.a, 255)); break;
		case SCEGU_DOUBLE_DST_ALPHA: bsel = glm::ivec4(2 * dest_color.a); break;
		case SCEGU_ONE_MINUS_DOUBLE_DST_ALPHA: bsel = glm::ivec4(255 - std::min(2 * dest_color.a, 255)); break;
		default: bsel = blend_bfix; break;
		}
	}

	glm::ivec4 result(0);
	switch (blend_operation) {
	case SCEGU_ADD:
		result = ((src_color * 2 + 1) * (asel * 2 + 1) / 1024) + ((dest_color * 2 + 1) * (bsel * 2 + 1) / 1024);
		break;
	case SCEGU_SUBTRACT:
		result = ((src_color * 2 + 1) * (asel * 2 + 1) / 1024) - ((dest_color * 2 + 1) * (bsel * 2 + 1) / 1024);
		break;
	case SCEGU_REVERSE_SUBTRACT:
		result = ((dest_color * 2 + 1) * (bsel * 2 + 1) / 1024) - ((src_color * 2 + 1) * (asel * 2 + 1) / 1024);
		break;
	case SCEGU_MIN: result = glm::min(dest_color, src_color); break;
	case SCEGU_MAX: result = glm::max(dest_color, src_color); break;
	case SCEGU_ABS: result = glm::abs(src_color - dest_color); break;
	}
	return { 0x00, 
		static_cast<uint8_t>(std::clamp(result.b, 0, 255)), 
		static_cast<uint8_t>(std::clamp(result.g, 0, 255)), 
		static_cast<uint8_t>(std::clamp(result.r, 0, 255)) };
}

Color SoftwareRenderer::BlendTexture(Color texture, Color blending) {
	glm::ivec4 tex_color{ texture.r, texture.g, texture.b, texture.a };
	glm::ivec4 blend_color{ blending.r, blending.g, blending.b, blending.a };

	glm::ivec4 result{};
	switch (texture_function) {
	case SCEGU_TEX_MODULATE:
		if (fragment_double) {
			result = ((blend_color + 1) * tex_color * 2) / 256;
		} else {
			result = (blend_color + 1) * tex_color / 256;
		}

		result.a = texture_alpha ? (blend_color.a + 1) * tex_color.a / 256 : blend_color.a;
		break;
	case SCEGU_TEX_DECAL:
		if (texture_alpha) {
			result = ((blend_color + 1) * (255 - tex_color.a) + (tex_color + 1) * tex_color.a);
			result /= fragment_double ? 128 : 256;
		} else {
			result = fragment_double ? tex_color * 2 : tex_color;
		}
		result.a = blend_color.a;
		break;
	case SCEGU_TEX_BLEND:
		result = ((255 - tex_color) * blend_color + tex_color * environment_texture + 255);
		result /= fragment_double ? 128 : 256;

		result.a = texture_alpha ? (blend_color.a + 1) * tex_color.a / 256 : blend_color.a;
		break;

	case SCEGU_TEX_REPLACE:
		result = tex_color;
		if (fragment_double) {
			result *= 2;
		}
		result.a = texture_alpha ? tex_color.a : blending.a;
		break;
	case SCEGU_TEX_ADD:
		result = tex_color + blend_color;
		if (fragment_double) {
			result *= 2;
		}
		result.a = texture_alpha ? (blend_color.a + 1) * tex_color.a / 256 : blend_color.a;
		break;
	}

	return { 
		static_cast<uint8_t>(std::clamp(result.a, 0, 255)),
		static_cast<uint8_t>(std::clamp(result.b, 0, 255)),
		static_cast<uint8_t>(std::clamp(result.g, 0, 255)),
		static_cast<uint8_t>(std::clamp(result.r, 0, 255)) };
}

glm::uvec2 SoftwareRenderer::FilterTexture(uint8_t filter, glm::vec2 uv) {
	auto& texture = textures[0];

	glm::uvec2 pos{};
	switch (filter) {
	case SCEGU_LINEAR:
	case SCEGU_NEAREST:
		pos.x = floorf(uv.x * texture.width);
		pos.y = floorf(uv.y * texture.height);
		break;
	default:
		spdlog::error("SoftwareRenderer: texel filter not implemented {}", filter);
	}
	pos.x = std::max(0u, std::min(pos.x, texture.width - 1));
	pos.y = std::max(0u, std::min(pos.y, texture.height - 1));
	return pos;
}

std::vector<Color> SoftwareRenderer::DecodeTexture() {
	std::vector<Color> texture_data{};
	
	auto& texture = textures[0];
	uint32_t size = texture.width * texture.height;
	texture_data.resize(size);

	auto psp = PSP::GetInstance();
	switch (texture_format) {
	case SCEGU_PF5650:
	case SCEGU_PF5551:
	case SCEGU_PF4444: {
		uint16_t* buffer = reinterpret_cast<uint16_t*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y++) {
			for (int x = 0; x < texture.width; x++) {
				uint16_t color = buffer[y * texture.pitch + x];
				if (texture_format == SCEGU_PF5650) {
					texture_data[y * texture.width + x] = BGR565ToABGR8888(color);
				} else if (texture_format == SCEGU_PF5551) {
					texture_data[y * texture.width + x] = ABGR1555ToABGR8888(color);
				} else if (texture_format == SCEGU_PF4444) {
					texture_data[y * texture.width + x] = ABGR4444ToABGR8888(color);
				}
			}
		}
		break;
	}
	case SCEGU_PF8888: {
		uint32_t* buffer = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y++) {
			memcpy(&texture_data.data()[y * texture.width], &buffer[y * texture.pitch], texture.width * 4);
		}
		break;
	}
	case SCEGU_PFIDX4: {
		uint8_t* buffer = reinterpret_cast<uint8_t*>(psp->VirtualToPhysical(texture.buffer));

		for (int y = 0; y < texture.height; y++) {
			int texture_index = y * texture.width;
			int clut_index = y * texture.pitch / 2;
			for (int x = 0; x < texture.width; x += 2, clut_index++) {
				uint8_t value = buffer[clut_index];
				texture_data[texture_index + x] = GetCLUT(value & 0xF);
				texture_data[texture_index + x + 1] = GetCLUT(value >> 4);
			}
		}
		break;
	}
	case SCEGU_PFIDX8: {
		uint8_t* buffer = reinterpret_cast<uint8_t*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y++) {
			int texture_index = y * texture.width;
			int clut_index = y * texture.pitch;
			for (int x = 0; x < texture.width; x++) {
				uint8_t index = buffer[clut_index + x];
				texture_data[texture_index + x] = GetCLUT(index);
			}
		}
		break;
	}
	case SCEGU_PFIDX32: {
		uint32_t* buffer = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y++) {
			int texture_index = y * texture.width;
			int clut_index = y * texture.pitch;
			for (int x = 0; x < texture.width; x++) {
				uint32_t index = buffer[clut_index + x];
				texture_data[texture_index + x] = GetCLUT(index);
			}
		}
		break;
	}
	case SCEGU_PFDXT1: {
		const DXT1Block* block_start = reinterpret_cast<DXT1Block*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y += 4) {
			const DXT1Block* block = block_start + (y / 4) * (texture.pitch / 4);
			for (int x = 0; x < texture.width; x += 4) {
				Color* dst = &texture_data.data()[y * texture.width + x];

				glm::ivec4 colors[4];
				DecodeDXTColors(block, colors, false);
				WriteDXT1(block, colors, dst, texture.pitch);
				block++;
			}
		}
		break;
	}
	case SCEGU_PFDXT3: {
		const DXT3Block* block_start = reinterpret_cast<DXT3Block*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y += 4) {
			const DXT3Block* block = block_start + (y / 4) * (texture.pitch / 4);
			for (int x = 0; x < texture.width; x += 4) {
				Color* dst = &texture_data.data()[y * texture.width + x];

				glm::ivec4 colors[4];
				DecodeDXTColors(&block->color, colors, true);
				WriteDXT3(block, colors, dst, texture.pitch);
				block++;
			}
		}
		break;
	}
	case SCEGU_PFDXT5: {
		const DXT5Block* block_start = reinterpret_cast<DXT5Block*>(psp->VirtualToPhysical(texture.buffer));
		for (int y = 0; y < texture.height; y += 4) {
			const DXT5Block* block = block_start + (y / 4) * (texture.pitch / 4);
			for (int x = 0; x < texture.width; x += 4) {
				Color* dst = &texture_data.data()[y * texture.width + x];

				glm::ivec4 colors[4];
				DecodeDXTColors(&block->color, colors, true);
				WriteDXT5(block, colors, dst, texture.pitch);
				block++;
			}
		}
		break;
	}
	default:
		spdlog::error("SoftwareRenderer: unknown texture format {}", texture_format);
	}

	return texture_data;
}