#pragma once

#include "../renderer.hpp"

#include <webgpu/webgpu.hpp>

class ComputeRenderer : public Renderer {
public:
	ComputeRenderer();
	~ComputeRenderer();

	void Frame();
	void Resize(int width, int height);
	void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format);
	void DrawRectangle(Vertex start, Vertex end) {}
	void DrawTriangle(Vertex v0, Vertex v1, Vertex v2) {}
	void DrawTriangleStrip(std::vector<Vertex> vertices) {}
	void DrawTriangleFan(std::vector<Vertex> vertices) {}
	void ClearTextureCache() {}
	void ClearTextureCache(uint32_t addr, uint32_t size) {}
private:
	struct ScreenTransform {
		glm::vec2 scale;
		glm::vec2 offset;
	};

	wgpu::Instance instance;
	wgpu::Surface surface;
	wgpu::Adapter adapter;
	wgpu::Device device;
	wgpu::Queue queue;
	wgpu::Buffer vertex_buffer;
	wgpu::Buffer index_buffer;
	wgpu::BindGroupLayout bind_group_layout;
	wgpu::PipelineLayout layout;
	wgpu::RenderPipeline render_pipeline;
	wgpu::BindGroup bind_group;

	wgpu::Buffer frame_width_buffer{};
	wgpu::Sampler nearest_sampler{};
	wgpu::Texture framebuffer_texture;
	uint32_t frame_buffer = 0;
	int pixel_format = 0;
	int frame_width = 0;

	int viewport_width = 0;
	int viewport_height = 0;
	int viewport_x = 0;
	int viewport_y = 0;
};