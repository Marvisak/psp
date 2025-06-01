#pragma once

#include "../renderer.hpp"

#include <unordered_map>
#include <webgpu/webgpu.hpp>

constexpr auto MAX_BUFFER_VERTEX_COUNT = 16384;

class ComputeRenderer : public Renderer {
public:
	ComputeRenderer();
	~ComputeRenderer();

	void Frame();
	void Resize(int width, int height);
	void RenderFramebufferChange() { compute_texture_valid = false; }
	void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format);
	void DrawRectangle(Vertex start, Vertex end);
	void DrawTriangle(Vertex v0, Vertex v1, Vertex v2);
	void DrawTriangleStrip(std::vector<Vertex> vertices);
	void DrawTriangleFan(std::vector<Vertex> vertices);
	void ClearTextureCache();
	void ClearTextureCache(uint32_t addr, uint32_t size);
	void FlushRender();
	void CLoad(uint32_t opcode);
private:
	struct ComputeVertex {
        alignas(16) glm::vec4 pos;
		alignas(8)  glm::vec2 uv;
		alignas(16) glm::uvec4 color;
	};

	struct RenderData {
		alignas(8) glm::uvec2 scissor_start;
		alignas(8) glm::uvec2 scissor_end;
		alignas(4) uint32_t clut_shift;
		alignas(4) uint32_t clut_mask;
		alignas(4) uint32_t clut_offset;
		alignas(16) glm::ivec4 blend_afix;
		alignas(16) glm::ivec4 blend_bfix;
		alignas(4) uint32_t alphaMask;
		alignas(4) uint32_t alphaRef;
		alignas(16) glm::ivec4 environment_texture;
	};

	struct TextureCacheEntry {
		int unused_frames;
		uint32_t size;
		wgpu::Texture texture;
		wgpu::BindGroup bind_group;
	};

	struct ClutCacheEntry {
		wgpu::Texture texture;
		wgpu::BindGroup bind_group;
	};

	union ShaderID {
		uint32_t full;
		struct {
			uint8_t primitive_type : 3;
			bool textures_enabled : 1;
			uint8_t texture_format : 4;
			bool u_clamp : 1;
			bool v_clamp : 1;
			uint8_t clut_format : 2;
			bool blend_enabled : 1;
			uint8_t blend_operation : 3;
			uint8_t blend_source : 4;
			uint8_t blend_destination : 4;
			bool alpha_test : 1;
			uint8_t alpha_func : 3;
			bool texture_alpha : 1;
			bool fragment_double : 1;
			uint8_t texture_function : 3;
		};
	};

	wgpu::ShaderModule CreateShaderModule(wgpu::StringView code);
	wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage);

	void SetupRenderPipeline(wgpu::ShaderModule shader_module);
	void SetupRenderBindGroup();
	void SetupFramebufferConversion(wgpu::ShaderModule shader_module);

	wgpu::ComputePipeline GetShader(uint8_t primitive_type);
	void UpdateRenderTexture();
	void UpdateRenderData();
	uint32_t PushVertices(std::vector<Vertex> vertices);
	wgpu::BindGroup GetTexture();

	wgpu::Instance instance;
	wgpu::Surface surface;
	wgpu::Adapter adapter;
	wgpu::Device device;
	wgpu::Queue queue;
	wgpu::Buffer vertex_buffer;
	wgpu::Buffer index_buffer;
	wgpu::BindGroupLayout render_bind_group_layout;
	wgpu::PipelineLayout render_layout;
	wgpu::RenderPipeline render_pipeline;
	wgpu::BindGroup render_bind_group;
	uint32_t buffer_alignment = 0;

	wgpu::Texture framebuffer_conversion_texture;
	wgpu::BindGroupLayout framebuffer_conversion_bind_group_layout;
	wgpu::PipelineLayout framebuffer_conversion_layout;
	wgpu::BindGroup framebuffer_conversion_bind_group;
	wgpu::ComputePipeline framebuffer_conversion_pipelines[3];

	std::unordered_map<uint32_t, wgpu::ComputePipeline> compute_pipelines{};
	bool queue_empty = true;
	wgpu::CommandEncoder compute_encoder;
	wgpu::ComputePassEncoder compute_pass_encoder;
	wgpu::BindGroupLayout compute_texture_bind_group_layout;
	wgpu::BindGroupLayout compute_clut_bind_group_layout;
	wgpu::BindGroupLayout compute_buffer_bind_group_layout;
	wgpu::BindGroup compute_buffer_bind_group;
	wgpu::BindGroupLayout compute_render_data_bind_group_layout;
	wgpu::BindGroup compute_render_data_bind_group;
	wgpu::PipelineLayout compute_layout;
	wgpu::PipelineLayout compute_texture_layout;
	bool compute_texture_valid = false;
	wgpu::Texture compute_texture;
	void* compute_vertices;
	uint32_t compute_vertex_buffer_offset = 0;
	wgpu::Buffer compute_vertex_buffer;
	wgpu::Buffer compute_render_data_buffer{};
	wgpu::Buffer compute_transitional_buffer;
	uint32_t current_fbp = 0;

	std::unordered_map<uint32_t, TextureCacheEntry> texture_cache{};
	std::vector<TextureCacheEntry> deleted_textures{};
	std::unordered_map<uint32_t, ClutCacheEntry> clut_cache{};
	uint32_t current_clut = 0;

	wgpu::Buffer frame_width_buffer;
	wgpu::Sampler nearest_sampler;
	wgpu::Texture framebuffer_texture;
	uint32_t frame_buffer = 0;
	int pixel_format = 0;
	int frame_width = 0;

	int viewport_width = 0;
	int viewport_height = 0;
	int viewport_x = 0;
	int viewport_y = 0;
};