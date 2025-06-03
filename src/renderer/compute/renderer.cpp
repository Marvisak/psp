#define WEBGPU_CPP_IMPLEMENTATION

#include "renderer.hpp"

#include <spdlog/spdlog.h>
#include <sdl3webgpu.h>

#include "../../psp.hpp"
#include "../../hle/defs.hpp"
#include "shaders/shaders.hpp"

float QUAD_VERTICES[]{
	-1.0f, -1.0f, 0.0f, 1.0f,
	1.0f, -1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 1.0f, 0.0f
};

uint16_t QUAD_INDICES[]{
	0, 1, 2,
	2, 1, 3
};

ComputeRenderer::ComputeRenderer(bool nearest_filtering) : Renderer() {
	instance = wgpu::createInstance();

	surface = SDL_GetWGPUSurface(instance, window);

	wgpu::RequestAdapterOptions adapter_options{};
	adapter_options.compatibleSurface = surface;
	adapter_options.powerPreference = wgpu::PowerPreference::HighPerformance;
	adapter = instance.requestAdapter(adapter_options);

	wgpu::AdapterInfo adapter_info{};
	if (adapter.getInfo(&adapter_info) != wgpu::Status::Success) {
		spdlog::error("ComputeRenderer: error when accessing adapter info");
	}
	else {
		spdlog::info("ComputeRenderer: Using {}", std::string(adapter_info.device.data, adapter_info.device.length));
	}

	WGPUFeatureName features[] = {
		static_cast<WGPUFeatureName>(WGPUNativeFeature_TextureAdapterSpecificFormatFeatures)
	};

	wgpu::DeviceDescriptor device_desc{};
	device_desc.requiredFeatures = features;
	device_desc.requiredFeatureCount = 1;
	device = adapter.requestDevice(device_desc);

	wgpu::Limits limits;
	if (device.getLimits(&limits) != wgpu::Status::Success) {
		spdlog::error("ComputeRenderer: error when accessing adapter info");
	} else {
		buffer_alignment = limits.minUniformBufferOffsetAlignment;
	}

	queue = device.getQueue();

	vertex_buffer = CreateBuffer(sizeof(QUAD_VERTICES), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst);
	index_buffer = CreateBuffer(sizeof(QUAD_INDICES), wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst);
	queue.writeBuffer(vertex_buffer, 0, QUAD_VERTICES, sizeof(QUAD_VERTICES));
	queue.writeBuffer(index_buffer, 0, QUAD_INDICES, sizeof(QUAD_INDICES));

	auto framebuffer_shader_module = CreateShaderModule(wgpu::StringView(framebuffer_shader));
	SetupRenderPipeline(framebuffer_shader_module);
	SetupRenderBindGroup(nearest_filtering);
	SetupFramebufferConversion(framebuffer_shader_module);
	framebuffer_shader_module.release();

	wgpu::BindGroupLayoutEntry compute_texture_binding_layout{};
	compute_texture_binding_layout.binding = 0;
	compute_texture_binding_layout.visibility = wgpu::ShaderStage::Compute;
	compute_texture_binding_layout.texture.sampleType = wgpu::TextureSampleType::Uint;
	compute_texture_binding_layout.texture.viewDimension = wgpu::TextureViewDimension::_2D;

	wgpu::BindGroupLayoutDescriptor compute_texture_bind_group_layout_desc{};
	compute_texture_bind_group_layout_desc.entryCount = 1;
	compute_texture_bind_group_layout_desc.entries = &compute_texture_binding_layout;
	compute_texture_bind_group_layout = device.createBindGroupLayout(compute_texture_bind_group_layout_desc);

	wgpu::BindGroupLayoutEntry compute_clut_binding_layout{};
	compute_clut_binding_layout.binding = 0;
	compute_clut_binding_layout.visibility = wgpu::ShaderStage::Compute;
	compute_clut_binding_layout.texture.sampleType = wgpu::TextureSampleType::Uint;
	compute_clut_binding_layout.texture.viewDimension = wgpu::TextureViewDimension::_1D;

	wgpu::BindGroupLayoutDescriptor compute_clut_bind_group_layout_desc{};
	compute_clut_bind_group_layout_desc.entryCount = 1;
	compute_clut_bind_group_layout_desc.entries = &compute_clut_binding_layout;
	compute_clut_bind_group_layout = device.createBindGroupLayout(compute_clut_bind_group_layout_desc);

	wgpu::BindGroupLayoutEntry compute_buffer_binding_layouts[2]{};
	compute_buffer_binding_layouts[0].binding = 0;
	compute_buffer_binding_layouts[0].visibility = wgpu::ShaderStage::Compute;
	compute_buffer_binding_layouts[0].storageTexture.access = wgpu::StorageTextureAccess::ReadWrite;
	compute_buffer_binding_layouts[0].storageTexture.format = wgpu::TextureFormat::RGBA8Uint;
	compute_buffer_binding_layouts[0].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

	compute_buffer_binding_layouts[1].binding = 1;
	compute_buffer_binding_layouts[1].visibility = wgpu::ShaderStage::Compute;
	compute_buffer_binding_layouts[1].storageTexture.access = wgpu::StorageTextureAccess::ReadWrite;
	compute_buffer_binding_layouts[1].storageTexture.format = wgpu::TextureFormat::R16Uint;
	compute_buffer_binding_layouts[1].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

	wgpu::BindGroupLayoutDescriptor compute_buffer_bind_group_layout_desc{};
	compute_buffer_bind_group_layout_desc.entryCount = 2;
	compute_buffer_bind_group_layout_desc.entries = compute_buffer_binding_layouts;
	compute_buffer_bind_group_layout = device.createBindGroupLayout(compute_buffer_bind_group_layout_desc);

	wgpu::BindGroupLayoutEntry compute_render_data_binding_layouts[2]{};
	compute_render_data_binding_layouts[0].binding = 0;
	compute_render_data_binding_layouts[0].visibility = wgpu::ShaderStage::Compute;
	compute_render_data_binding_layouts[0].buffer.type = wgpu::BufferBindingType::Uniform;
	compute_render_data_binding_layouts[0].buffer.minBindingSize = sizeof(RenderData);

	compute_render_data_binding_layouts[1].binding = 1;
	compute_render_data_binding_layouts[1].visibility = wgpu::ShaderStage::Compute;
	compute_render_data_binding_layouts[1].buffer.type = wgpu::BufferBindingType::Uniform;
	compute_render_data_binding_layouts[1].buffer.hasDynamicOffset = true;
	compute_render_data_binding_layouts[1].buffer.minBindingSize = sizeof(ComputeVertex) * 4;

	wgpu::BindGroupLayoutDescriptor compute_render_data_bind_group_layout_desc{};
	compute_render_data_bind_group_layout_desc.entryCount = 2;
	compute_render_data_bind_group_layout_desc.entries = compute_render_data_binding_layouts;
	compute_render_data_bind_group_layout = device.createBindGroupLayout(compute_render_data_bind_group_layout_desc);

	WGPUBindGroupLayout compute_bind_group_layouts[] = {
		compute_buffer_bind_group_layout,
		compute_render_data_bind_group_layout,
		compute_texture_bind_group_layout,
		compute_clut_bind_group_layout
	};

	wgpu::PipelineLayoutDescriptor compute_texture_layout_desc{};
	compute_texture_layout_desc.bindGroupLayoutCount = 4;
	compute_texture_layout_desc.bindGroupLayouts = compute_bind_group_layouts;
	compute_texture_layout = device.createPipelineLayout(compute_texture_layout_desc);

	wgpu::PipelineLayoutDescriptor compute_layout_desc{};
	compute_layout_desc.bindGroupLayoutCount = 2;
	compute_layout_desc.bindGroupLayouts = compute_bind_group_layouts;
	compute_layout = device.createPipelineLayout(compute_layout_desc);

	compute_render_data_buffer = CreateBuffer(sizeof(RenderData), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);
	compute_vertex_buffer = CreateBuffer(sizeof(ComputeVertex) * MAX_BUFFER_VERTEX_COUNT, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);
	compute_vertices = operator new(sizeof(ComputeVertex) * MAX_BUFFER_VERTEX_COUNT);

	wgpu::BindGroupEntry compute_render_data_bindings[2]{};
	compute_render_data_bindings[0].binding = 0;
	compute_render_data_bindings[0].buffer = compute_render_data_buffer;
	compute_render_data_bindings[0].size = sizeof(RenderData);

	compute_render_data_bindings[1].binding = 1;
	compute_render_data_bindings[1].buffer = compute_vertex_buffer;
	compute_render_data_bindings[1].size = sizeof(ComputeVertex) * 4;

	wgpu::BindGroupDescriptor compute_render_data_bind_group_desc{};
	compute_render_data_bind_group_desc.layout = compute_render_data_bind_group_layout;
	compute_render_data_bind_group_desc.entryCount = 2;
	compute_render_data_bind_group_desc.entries = compute_render_data_bindings;
	compute_render_data_bind_group = device.createBindGroup(compute_render_data_bind_group_desc);

	compute_transitional_buffer = CreateBuffer(512 * 512 * 4, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);
	compute_depth_transitional_buffer = CreateBuffer(512 * 512 * 2, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);

	Resize(BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT);

	compute_encoder = device.createCommandEncoder();
	compute_pass_encoder = compute_encoder.beginComputePass();
}

ComputeRenderer::~ComputeRenderer() {
	for (int i = SCE_DISPLAY_PIXEL_RGB565; i < SCE_DISPLAY_PIXEL_RGBA8888; i++) {
		framebuffer_conversion_pipelines[i].release();
	}

	for (auto& [_, entry] : texture_cache) {
		entry.bind_group.release();
		entry.texture.destroy();
	}

	for (auto& [_, entry] : clut_cache) {
		entry.bind_group.release();
		entry.texture.destroy();
	}

	delete[] compute_vertices;
	compute_encoder.release();
	compute_clut_bind_group_layout.release();
	compute_texture_bind_group_layout.release();
	compute_transitional_buffer.release();
	compute_depth_transitional_buffer.release();
	compute_render_data_bind_group.release();
	compute_render_data_bind_group_layout.release();
	if (compute_buffer_bind_group) {
		compute_buffer_bind_group.release();
	}
	compute_buffer_bind_group_layout.release();
	compute_layout.release();
	if (compute_texture) {
		compute_texture.release();
	}
	if (compute_depth_texture) {
		compute_depth_texture.release();
	}
	framebuffer_conversion_texture.release();
	framebuffer_conversion_bind_group.release();
	framebuffer_conversion_layout.release();
	framebuffer_conversion_bind_group_layout.release();
	frame_width_buffer.release();
	framebuffer_texture.release();
	screen_sampler.release();
	render_bind_group.release();
	render_pipeline.release();
	render_layout.release();
	render_bind_group_layout.release();
	index_buffer.release();
	vertex_buffer.release();
	surface.unconfigure();
	queue.release();
	device.release();
	adapter.release();
	surface.release();
	instance.release();
}

void ComputeRenderer::Frame() {
	wgpu::SurfaceTexture surface_texture{};
	surface.getCurrentTexture(&surface_texture);

	if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal && surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
		spdlog::error("ComputeRenderer: failed obtaining surface texture");
		return;
	}

	wgpu::TextureViewDescriptor view_descriptor{};
	view_descriptor.format = wgpuTextureGetFormat(surface_texture.texture);
	view_descriptor.dimension = wgpu::TextureViewDimension::_2D;
	view_descriptor.mipLevelCount = 1;
	view_descriptor.arrayLayerCount = 1;
	view_descriptor.aspect = wgpu::TextureAspect::All;
	wgpu::TextureView target_view = wgpuTextureCreateView(surface_texture.texture, &view_descriptor);

	auto encoder = device.createCommandEncoder();

	wgpu::RenderPassColorAttachment render_pass_color_attachment{};
	render_pass_color_attachment.view = target_view;
	render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
	render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
	render_pass_color_attachment.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 0.0 };

	wgpu::RenderPassDescriptor render_pass_desc{};
	render_pass_desc.colorAttachmentCount = 1;
	render_pass_desc.colorAttachments = &render_pass_color_attachment;

	FlushRender();
	if (frame_buffer) {
		void* framebuffer = PSP::GetInstance()->VirtualToPhysical(frame_buffer);

		if (pixel_format == SCE_DISPLAY_PIXEL_RGBA8888) {
			wgpu::TexelCopyTextureInfo destination{};
			destination.texture = framebuffer_texture;

			wgpu::TexelCopyBufferLayout data_layout{};
			data_layout.bytesPerRow = frame_width * 4;
			data_layout.rowsPerImage = BASE_HEIGHT;

			queue.writeTexture(destination, framebuffer, frame_width * BASE_HEIGHT * 4, data_layout, {std::min<uint32_t>(BASE_WIDTH, frame_width), BASE_HEIGHT, 1});
		} else {
			wgpu::TexelCopyTextureInfo destination{};
			destination.texture = framebuffer_conversion_texture;

			wgpu::TexelCopyBufferLayout data_layout{};
			data_layout.bytesPerRow = frame_width * 2;
			data_layout.rowsPerImage = BASE_HEIGHT;

			queue.writeTexture(destination, framebuffer, frame_width * BASE_HEIGHT * 2, data_layout, { std::min<uint32_t>(BASE_WIDTH, frame_width), BASE_HEIGHT, 1 });

			auto compute_pass = encoder.beginComputePass();

			compute_pass.setPipeline(framebuffer_conversion_pipelines[pixel_format]);
			compute_pass.setBindGroup(0, framebuffer_conversion_bind_group, 0, nullptr);

			uint32_t workgroup_count_x = (BASE_WIDTH + 7) / 8;
			uint32_t workgroup_count_y = (BASE_HEIGHT + 7) / 8;

			compute_pass.dispatchWorkgroups(workgroup_count_x, workgroup_count_y, 1);

			compute_pass.end();
			compute_pass.release();
		}
	}

	auto render_pass = encoder.beginRenderPass(render_pass_desc);
	if (frame_buffer) {
		render_pass.setViewport(viewport_x, viewport_y, viewport_width, viewport_height, 0.0f, 0.0f);
		render_pass.setPipeline(render_pipeline);
		render_pass.setVertexBuffer(0, vertex_buffer, 0, sizeof(QUAD_VERTICES));
		render_pass.setIndexBuffer(index_buffer, wgpu::IndexFormat::Uint16, 0, sizeof(QUAD_INDICES));
		render_pass.setBindGroup(0, render_bind_group, 0, nullptr);
		render_pass.drawIndexed(6, 1, 0, 0, 0);
	}

	render_pass.end();

	auto command = encoder.finish();
	queue.submit(command);
	surface.present();

	for (auto it = texture_cache.begin(); it != texture_cache.end();) {
		it->second.unused_frames++;
		if (it->second.unused_frames >= TEXTURE_CACHE_CLEAR_FRAMES) {
			deleted_textures.push_back(it->second);
			texture_cache.erase(it++);
		}
		else {
			it++;
		}
	}

	render_pass.release();
	encoder.release();
	command.release();
	target_view.release();
	wgpuTextureRelease(surface_texture.texture);

	Renderer::Frame();
}

void ComputeRenderer::Resize(int width, int height) {
	wgpu::SurfaceConfiguration surface_config{};
	surface_config.width = width;
	surface_config.height = height;
	surface_config.usage = wgpu::TextureUsage::RenderAttachment;
	surface_config.device = device;
	surface_config.format = wgpu::TextureFormat::RGBA8Unorm;
	surface_config.presentMode = wgpu::PresentMode::Immediate;
	surface.configure(surface_config);

	float scaleX = static_cast<float>(width) / BASE_WIDTH;
	float scaleY = static_cast<float>(height) / BASE_HEIGHT;
	float scale = fminf(scaleX, scaleY);

	viewport_width = BASE_WIDTH * scale;
	viewport_height = BASE_HEIGHT * scale;

	viewport_x = (width - viewport_width) / 2;
	viewport_y = (height - viewport_height) / 2;
}

void ComputeRenderer::SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format) {
	Renderer::SetFrameBuffer(frame_buffer, frame_width, pixel_format);

	this->frame_buffer = frame_buffer;
	this->pixel_format = pixel_format;

	if (this->frame_width != frame_width) {
		this->frame_width = frame_width;

		float buffer_data = frame_width;
		queue.writeBuffer(frame_width_buffer, 0, &buffer_data, sizeof(float));
	}
}

void ComputeRenderer::DrawRectangle(Vertex start, Vertex end) {
	if (!compute_texture_valid) {
		UpdateRenderTexture();
	}

	start.pos = glm::round(start.pos);
	end.pos = glm::round(end.pos);

	if (start.pos.x > end.pos.x) {
		std::swap(start.pos.x, end.pos.x);
		std::swap(start.uv.x, end.uv.x);
	}

	if (start.pos.y > end.pos.y) {
		std::swap(start.pos.y, end.pos.y);
		std::swap(start.uv.y, end.uv.y);
	}

	int width = end.pos.x - start.pos.x;
	int height = end.pos.y - start.pos.y;

	if (width == 0 || height == 0) {
		return;
	}

	auto filter = GetFilter((end.uv.x - start.uv.x) / width, (end.uv.y - start.uv.y) / height);
	auto pipeline = GetShader(SCEGU_PRIM_RECTANGLES, filter);

	auto offset = PushVertices({ start, end });

	compute_pass_encoder.setPipeline(pipeline);
	compute_pass_encoder.setBindGroup(0, compute_buffer_bind_group, 0, nullptr);
	compute_pass_encoder.setBindGroup(1, compute_render_data_bind_group, 1, &offset);

	if (!clear_mode && textures_enabled) {
		auto texture_bind_group = GetTexture();
		compute_pass_encoder.setBindGroup(2, texture_bind_group, 0, nullptr);
		compute_pass_encoder.setBindGroup(3, clut_cache[current_clut].bind_group, 0, nullptr);
	}

	uint32_t workgroup_count_x = (width + 7) / 8;
	uint32_t workgroup_count_y = (height + 7) / 8;

	compute_pass_encoder.dispatchWorkgroups(workgroup_count_x, workgroup_count_y, 1);

	queue_empty = false;
}

void ComputeRenderer::DrawTriangle(Vertex v0, Vertex v1, Vertex v2) {
	if (!compute_texture_valid) {
		UpdateRenderTexture();
	}

	v0.pos = glm::vec4(glm::round(glm::vec3(v0.pos)), v0.pos.w);
	v1.pos = glm::vec4(glm::round(glm::vec3(v1.pos)), v1.pos.w);
	v2.pos = glm::vec4(glm::round(glm::vec3(v2.pos)), v2.pos.w);

	float area = (v1.pos.x - v0.pos.x) * (v2.pos.y - v0.pos.y) - (v1.pos.y - v0.pos.y) * (v2.pos.x - v0.pos.x);
	if (area == 0) {
		return;
	}

	if (!clear_mode && culling) {
		switch (cull_type) {
		case SCEGU_CCW:
			if (area < 0) {
				return;
			}
			break;
		case SCEGU_CW:
			if (area > 0) {
				return;
			}
			std::swap(v0, v1);
			break;
		}
	} else if (area < 0) {
		std::swap(v0, v1);
	}

	int min_x = std::min({ v0.pos.x, v1.pos.x, v2.pos.x });
	int max_x = std::max({ v0.pos.x, v1.pos.x, v2.pos.x });
	int min_y = std::min({ v0.pos.y, v1.pos.y, v2.pos.y });
	int max_y = std::max({ v0.pos.y, v1.pos.y, v2.pos.y });

	// This is the most ghetto code I have ever written, but it does the job and makes everything simple
	Vertex bounding{};
	bounding.pos = glm::vec4(min_x, min_y, 0.0, 0.0);

	auto filter = GetFilter(v1.uv.x - v0.uv.x, v2.uv.y - v0.uv.y);
	auto pipeline = GetShader(SCEGU_PRIM_TRIANGLES, filter);

	auto offset = PushVertices({ v0, v1, v2, bounding });

	compute_pass_encoder.setPipeline(pipeline);
	compute_pass_encoder.setBindGroup(0, compute_buffer_bind_group, 0, nullptr);
	compute_pass_encoder.setBindGroup(1, compute_render_data_bind_group, 1, &offset);

	if (!clear_mode && textures_enabled) {
		auto texture_bind_group = GetTexture();
		compute_pass_encoder.setBindGroup(2, texture_bind_group, 0, nullptr);
		compute_pass_encoder.setBindGroup(3, clut_cache[current_clut].bind_group, 0, nullptr);
	}

	uint32_t workgroup_count_x = (max_x - min_x + 7) / 8;
	uint32_t workgroup_count_y = (max_y - min_y + 7) / 8;

	compute_pass_encoder.dispatchWorkgroups(workgroup_count_x, workgroup_count_y, 1);

	queue_empty = false;
}

void ComputeRenderer::DrawTriangleStrip(std::vector<Vertex> vertices) {
	for (int i = 2; i < vertices.size(); i++) {
		DrawTriangle(vertices[i - 2], vertices[i - 1], vertices[i]);
	}
}

void ComputeRenderer::DrawTriangleFan(std::vector<Vertex> vertices) {
	for (int i = 1; i < vertices.size() - 1; i++) {
		DrawTriangle(vertices[0], vertices[i], vertices[i + 1]);
	}
}

void ComputeRenderer::ClearTextureCache() {
	for (auto& [_, entry] : texture_cache) {
		entry.dirty = true;
	}
}

void ComputeRenderer::ClearTextureCache(uint32_t addr, uint32_t size) {
	addr &= 0x3FFFFFFF;
	uint32_t addr_end = addr + size;
	for (auto& [addr, entry] : texture_cache) {
		uint32_t texture_end = addr + entry.size / 2;
		if (addr <= texture_end && addr_end >= addr) {
			entry.dirty = true;
		}
	}
}

void ComputeRenderer::CLoad(uint32_t opcode) {
	Renderer::CLoad(opcode);

	uint64_t checksum = 14695981039346656037ULL;
	for (auto byte : clut) {
		checksum ^= byte;
		checksum *= 1099511628211ULL;
	}
	current_clut = checksum;

	if (clut_cache.contains(checksum)) {
		return;
	}

	ClutCacheEntry cache{};

	wgpu::TextureDescriptor texture_desc{};
	texture_desc.dimension = wgpu::TextureDimension::_1D;
	texture_desc.format = wgpu::TextureFormat::RGBA8Uint;
	texture_desc.size = { 256, 1, 1 };
	texture_desc.sampleCount = 1;
	texture_desc.mipLevelCount = 1;
	texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
	cache.texture = device.createTexture(texture_desc);

	wgpu::TexelCopyTextureInfo destination{};
	destination.texture = cache.texture;

	wgpu::TexelCopyBufferLayout data_layout{};
	data_layout.bytesPerRow = 1024;
	data_layout.rowsPerImage = 1;

	queue.writeTexture(destination, clut.data(), 1024, data_layout, { 256, 1, 1 });

	wgpu::BindGroupEntry texture_binding{};
	texture_binding.binding = 0;
	texture_binding.textureView = cache.texture.createView();

	wgpu::BindGroupDescriptor texture_bind_group_desc{};
	texture_bind_group_desc.layout = compute_clut_bind_group_layout;
	texture_bind_group_desc.entryCount = 1;
	texture_bind_group_desc.entries = &texture_binding;
	cache.bind_group = device.createBindGroup(texture_bind_group_desc);

	clut_cache[checksum] = cache;
}

wgpu::ShaderModule ComputeRenderer::CreateShaderModule(wgpu::StringView code) {
	wgpu::ShaderSourceWGSL shader_source{};
	shader_source.chain.sType = wgpu::SType::ShaderSourceWGSL;
	shader_source.code = code;

	wgpu::ShaderModuleDescriptor shader_desc{};
	shader_desc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&shader_source);

	return device.createShaderModule(shader_desc);
}

wgpu::Buffer ComputeRenderer::CreateBuffer(uint64_t size, wgpu::BufferUsage usage) {
	wgpu::BufferDescriptor buffer_desc{};
	buffer_desc.size = size;
	buffer_desc.usage = usage;
	return device.createBuffer(buffer_desc);
}

void ComputeRenderer::SetupRenderPipeline(wgpu::ShaderModule shader_module) {
	wgpu::VertexAttribute vertex_attrs[2]{};

	vertex_attrs[0].format = wgpu::VertexFormat::Float32x2;
	vertex_attrs[0].offset = 0;
	vertex_attrs[0].shaderLocation = 0;

	vertex_attrs[1].format = wgpu::VertexFormat::Float32x2;
	vertex_attrs[1].offset = sizeof(float) * 2;
	vertex_attrs[1].shaderLocation = 1;

	wgpu::VertexBufferLayout vertex_layout{};
	vertex_layout.arrayStride = sizeof(float) * 4;
	vertex_layout.stepMode = wgpu::VertexStepMode::Vertex;
	vertex_layout.attributeCount = 2;
	vertex_layout.attributes = vertex_attrs;

	wgpu::ColorTargetState color_target{};
	color_target.format = wgpu::TextureFormat::RGBA8Unorm;
	color_target.writeMask = wgpu::ColorWriteMask::All;

	wgpu::FragmentState fragment_state{};
	fragment_state.module = shader_module;
	fragment_state.entryPoint = wgpu::StringView("fsMain");
	fragment_state.targetCount = 1;
	fragment_state.targets = &color_target;

	wgpu::VertexState vertex_state{};
	vertex_state.module = shader_module;
	vertex_state.entryPoint = wgpu::StringView("vsMain");
	vertex_state.bufferCount = 1;
	vertex_state.buffers = &vertex_layout;

	wgpu::BindGroupLayoutEntry render_binding_layouts[3]{};

	render_binding_layouts[0].binding = 0;
	render_binding_layouts[0].visibility = wgpu::ShaderStage::Fragment;
	render_binding_layouts[0].texture.sampleType = wgpu::TextureSampleType::Float;
	render_binding_layouts[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;

	render_binding_layouts[1].binding = 1;
	render_binding_layouts[1].visibility = wgpu::ShaderStage::Fragment;
	render_binding_layouts[1].sampler.type = wgpu::SamplerBindingType::Filtering;

	render_binding_layouts[2].binding = 2;
	render_binding_layouts[2].visibility = wgpu::ShaderStage::Fragment;
	render_binding_layouts[2].buffer.type = wgpu::BufferBindingType::Uniform;
	render_binding_layouts[2].buffer.minBindingSize = sizeof(float);

	wgpu::BindGroupLayoutDescriptor render_bind_group_layout_desc{};
	render_bind_group_layout_desc.entryCount = 3;
	render_bind_group_layout_desc.entries = render_binding_layouts;
	render_bind_group_layout = device.createBindGroupLayout(render_bind_group_layout_desc);

	wgpu::PipelineLayoutDescriptor render_layout_desc{};
	render_layout_desc.bindGroupLayoutCount = 1;
	render_layout_desc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&render_bind_group_layout);
	render_layout = device.createPipelineLayout(render_layout_desc);

	wgpu::RenderPipelineDescriptor pipeline_desc{};
	pipeline_desc.vertex = vertex_state;
	pipeline_desc.fragment = &fragment_state;
	pipeline_desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	pipeline_desc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipeline_desc.primitive.cullMode = wgpu::CullMode::None;
	pipeline_desc.multisample.count = 1;
	pipeline_desc.multisample.mask = 0xFFFFFFFF;
	pipeline_desc.layout = render_layout;

	render_pipeline = device.createRenderPipeline(pipeline_desc);
}

void ComputeRenderer::SetupRenderBindGroup(bool nearest_filtering) {
	wgpu::TextureDescriptor texture_desc{};
	texture_desc.dimension = wgpu::TextureDimension::_2D;
	texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
	texture_desc.size = { BASE_WIDTH, BASE_HEIGHT, 1 };
	texture_desc.sampleCount = 1;
	texture_desc.mipLevelCount = 1;
	texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::StorageBinding;
	framebuffer_texture = device.createTexture(texture_desc);

	wgpu::SamplerDescriptor sampler_desc{};
	sampler_desc.addressModeU = wgpu::AddressMode::Repeat;
	sampler_desc.addressModeV = wgpu::AddressMode::Repeat;
	sampler_desc.magFilter = nearest_filtering ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
	sampler_desc.minFilter = nearest_filtering ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
	sampler_desc.maxAnisotropy = 1;
	screen_sampler = device.createSampler(sampler_desc);

	frame_width_buffer = CreateBuffer(sizeof(float), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

	wgpu::BindGroupEntry render_bindings[3]{};
	render_bindings[0].binding = 0;
	render_bindings[0].textureView = framebuffer_texture.createView();

	render_bindings[1].binding = 1;
	render_bindings[1].sampler = screen_sampler;

	render_bindings[2].binding = 2;
	render_bindings[2].buffer = frame_width_buffer;
	render_bindings[2].size = sizeof(float);

	wgpu::BindGroupDescriptor bind_group_desc{};
	bind_group_desc.layout = render_bind_group_layout;
	bind_group_desc.entryCount = 3;
	bind_group_desc.entries = render_bindings;
	render_bind_group = device.createBindGroup(bind_group_desc);
}

void ComputeRenderer::SetupFramebufferConversion(wgpu::ShaderModule shader_module) {
	wgpu::BindGroupLayoutEntry framebuffer_conversion_binding_layouts[2]{};
	framebuffer_conversion_binding_layouts[0].binding = 0;
	framebuffer_conversion_binding_layouts[0].visibility = wgpu::ShaderStage::Compute;
	framebuffer_conversion_binding_layouts[0].texture.sampleType = wgpu::TextureSampleType::Uint;
	framebuffer_conversion_binding_layouts[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;

	framebuffer_conversion_binding_layouts[1].binding = 1;
	framebuffer_conversion_binding_layouts[1].visibility = wgpu::ShaderStage::Compute;
	framebuffer_conversion_binding_layouts[1].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
	framebuffer_conversion_binding_layouts[1].storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
	framebuffer_conversion_binding_layouts[1].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

	wgpu::BindGroupLayoutDescriptor framebuffer_conversion_bind_group_layout_desc{};
	framebuffer_conversion_bind_group_layout_desc.entryCount = 2;
	framebuffer_conversion_bind_group_layout_desc.entries = framebuffer_conversion_binding_layouts;
	framebuffer_conversion_bind_group_layout = device.createBindGroupLayout(framebuffer_conversion_bind_group_layout_desc);

	const char* compute_entrypoints[] = {
		"rgb565",
		"rgba5551",
		"rgba4444"
	};

	wgpu::PipelineLayoutDescriptor framebuffer_conversion_layout_desc{};
	framebuffer_conversion_layout_desc.bindGroupLayoutCount = 1;
	framebuffer_conversion_layout_desc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&framebuffer_conversion_bind_group_layout);
	framebuffer_conversion_layout = device.createPipelineLayout(framebuffer_conversion_layout_desc);

	for (int i = SCE_DISPLAY_PIXEL_RGB565; i < SCE_DISPLAY_PIXEL_RGBA8888; i++) {
		wgpu::ComputePipelineDescriptor compute_pipeline_desc{};
		compute_pipeline_desc.compute.entryPoint = wgpu::StringView(compute_entrypoints[i]);
		compute_pipeline_desc.compute.module = shader_module;
		compute_pipeline_desc.layout = framebuffer_conversion_layout;
		framebuffer_conversion_pipelines[i] = device.createComputePipeline(compute_pipeline_desc);
	}

	wgpu::TextureDescriptor framebuffer_conversion_texture_desc{};
	framebuffer_conversion_texture_desc.dimension = wgpu::TextureDimension::_2D;
	framebuffer_conversion_texture_desc.format = wgpu::TextureFormat::R16Uint;
	framebuffer_conversion_texture_desc.size = { BASE_WIDTH, BASE_HEIGHT, 1 };
	framebuffer_conversion_texture_desc.sampleCount = 1;
	framebuffer_conversion_texture_desc.mipLevelCount = 1;
	framebuffer_conversion_texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
	framebuffer_conversion_texture = device.createTexture(framebuffer_conversion_texture_desc);

	wgpu::BindGroupEntry framebuffer_conversion_bindings[2]{};
	framebuffer_conversion_bindings[0].binding = 0;
	framebuffer_conversion_bindings[0].textureView = framebuffer_conversion_texture.createView();

	framebuffer_conversion_bindings[1].binding = 1;
	framebuffer_conversion_bindings[1].textureView = framebuffer_texture.createView();

	wgpu::BindGroupDescriptor framebuffer_conversion_bind_group_desc{};
	framebuffer_conversion_bind_group_desc.layout = framebuffer_conversion_bind_group_layout;
	framebuffer_conversion_bind_group_desc.entryCount = 2;
	framebuffer_conversion_bind_group_desc.entries = framebuffer_conversion_bindings;
	framebuffer_conversion_bind_group = device.createBindGroup(framebuffer_conversion_bind_group_desc);
}

wgpu::ComputePipeline ComputeRenderer::GetShader(uint8_t primitive_type, uint8_t filter) {
	ShaderID id{};

	id.primitive_type = primitive_type;
	if (!clear_mode) {
		id.textures_enabled = textures_enabled;
		if (textures_enabled) {
			id.bilinear = (filter & 1) != 0;
			id.texture_format = texture_format;
			id.u_clamp = u_clamp;
			id.v_clamp = v_clamp;
			id.fragment_double = fragment_double;
			id.texture_alpha = texture_alpha;
			id.texture_function = texture_function;
			if (texture_format >= SCEGU_PFIDX4 && texture_format <= SCEGU_PFIDX32) {
				id.clut_format = clut_format;
			}
		}

		id.blend_enabled = blend;
		if (blend) {
			id.blend_operation = blend_operation;
			id.blend_source = blend_source;
			id.blend_destination = blend_destination;
		}

		id.alpha_test = alpha_test;
		if (alpha_test) {
			id.alpha_func = alpha_test_func;
		}

		id.depth_test = depth_test;
		if (depth_test) {
			id.depth_func = depth_test_func;
		}
		id.depth_write = depth_write;

		id.gouraud_shading = gouraud_shading;
	} else {
		id.depth_write = clear_mode_depth;
	}

	if (compute_pipelines.contains(id.full)) {
		return compute_pipelines[id.full];
	}

	WGPUConstantEntry constants[] {
		{ .key = wgpu::StringView("TEXTURE_FORMAT"), .value = id.textures_enabled ? texture_format : 100.0f },
		{ .key = wgpu::StringView("BILINEAR"), .value = id.bilinear ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("U_CLAMP"), .value = id.u_clamp ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("V_CLAMP"), .value = id.v_clamp ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("CLUT_FORMAT"), .value = static_cast<double>(clut_format) },
		{ .key = wgpu::StringView("BLEND_OPERATION"), .value = id.blend_enabled ? blend_operation : 100.0f },
		{ .key = wgpu::StringView("BLEND_SOURCE"), .value = static_cast<double>(blend_source) },
		{ .key = wgpu::StringView("BLEND_DESTINATION"), .value = static_cast<double>(blend_destination) },
		{ .key = wgpu::StringView("ALPHA_FUNC"), .value = id.alpha_test ? alpha_test_func : 100.0f },
		{ .key = wgpu::StringView("USE_TEXTURE_ALPHA"), .value = id.texture_alpha ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("FRAGMENT_DOUBLE"), .value = id.fragment_double ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("TEXTURE_FUNCTION"), .value = static_cast<double>(texture_function) },
		{ .key = wgpu::StringView("DEPTH_FUNC"), .value = id.depth_test ? depth_test_func : 100.0f },
		{ .key = wgpu::StringView("DEPTH_WRITE"), .value = id.depth_write ? 1.0f : 0.0f },
		{ .key = wgpu::StringView("GOURAUD_SHADING"), .value = id.gouraud_shading ? 1.0f : 0.0f },

	};

	std::string code = common_shader;
	if (id.textures_enabled) {
		code += texture_shader;
	} else {
		code += "fn filterTexture(uv: vec2f, color: vec4u) -> vec4u { return vec4u(0, 0, 0, 0); }";
	}

	code += pixel_shader;

	switch (primitive_type) {
	case SCEGU_PRIM_TRIANGLES:
		code += triangle_shader;
		break;
	case SCEGU_PRIM_RECTANGLES:
		code += rectangle_shader;
		break;
	}

	auto shader_module = CreateShaderModule(wgpu::StringView(code));

	wgpu::ComputePipelineDescriptor compute_pipeline_desc{};
	compute_pipeline_desc.compute.entryPoint = wgpu::StringView("draw");
	compute_pipeline_desc.compute.module = shader_module;
	compute_pipeline_desc.compute.constants = constants;
	compute_pipeline_desc.compute.constantCount = 15;
	compute_pipeline_desc.layout = id.textures_enabled ? compute_texture_layout : compute_layout;
	auto compute_pipeline = device.createComputePipeline(compute_pipeline_desc);
	shader_module.release();

	compute_pipelines[id.full] = compute_pipeline;

	return compute_pipeline;
}

void ComputeRenderer::UpdateRenderTexture() {
	auto format = fpf == SCE_DISPLAY_PIXEL_RGBA8888 ? wgpu::TextureFormat::RGBA8Uint : wgpu::TextureFormat::R16Uint;

	bool create_texture = true;
	if (compute_texture && compute_depth_texture) {
		FlushRender();

		if (compute_texture.getFormat() == format && compute_texture.getWidth() == fbw
			&& compute_depth_texture.getWidth() == zbw) {
			create_texture = false;
		} else {
			compute_texture.release();
			compute_depth_texture.release();
		}
	}

	current_fbp = fbp;
	current_zbp = zbp;
	if (create_texture) {
		wgpu::TextureDescriptor texture_desc{};
		texture_desc.dimension = wgpu::TextureDimension::_2D;
		texture_desc.format = format;
		texture_desc.size = { fbw, 512, 1 };
		texture_desc.sampleCount = 1;
		texture_desc.mipLevelCount = 1;
		texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::StorageBinding;
		compute_texture = device.createTexture(texture_desc);

		wgpu::TextureDescriptor depth_buffer_texture_desc{};
		depth_buffer_texture_desc.dimension = wgpu::TextureDimension::_2D;
		depth_buffer_texture_desc.format = wgpu::TextureFormat::R16Uint;
		depth_buffer_texture_desc.size = { zbw, 512, 1 };
		depth_buffer_texture_desc.sampleCount = 1;
		depth_buffer_texture_desc.mipLevelCount = 1;
		depth_buffer_texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::StorageBinding;
		compute_depth_texture = device.createTexture(depth_buffer_texture_desc);

		if (compute_buffer_bind_group) {
			compute_buffer_bind_group.release();
		}

		wgpu::BindGroupEntry compute_buffer_bindings[2]{};
		compute_buffer_bindings[0].binding = 0;
		compute_buffer_bindings[0].textureView = compute_texture.createView();

		compute_buffer_bindings[1].binding = 1;
		compute_buffer_bindings[1].textureView = compute_depth_texture.createView();

		wgpu::BindGroupDescriptor compute_bind_group_desc{};
		compute_bind_group_desc.layout = compute_buffer_bind_group_layout;
		compute_bind_group_desc.entryCount = 2;
		compute_bind_group_desc.entries = compute_buffer_bindings;
		compute_buffer_bind_group = device.createBindGroup(compute_bind_group_desc);
	}

	int bpp = fpf == SCE_DISPLAY_PIXEL_RGBA8888 ? 4 : 2;

	wgpu::TexelCopyTextureInfo destination{};
	destination.texture = compute_texture;

	wgpu::TexelCopyBufferLayout data_layout{};
	data_layout.bytesPerRow = fbw * bpp;
	data_layout.rowsPerImage = 512;

	auto framebuffer = PSP::GetInstance()->VirtualToPhysical(GetFrameBufferAddress());

	queue.writeTexture(destination, framebuffer, fbw * 512 * bpp, data_layout, { fbw, 512, 1 });

	wgpu::TexelCopyTextureInfo depth_destination{};
	depth_destination.texture = compute_depth_texture;

	wgpu::TexelCopyBufferLayout depth_data_layout{};
	depth_data_layout.bytesPerRow = zbw * 2;
	depth_data_layout.rowsPerImage = 512;

	auto depth_buffer = PSP::GetInstance()->VirtualToPhysical(GetDepthBufferAddress());
	queue.writeTexture(depth_destination, depth_buffer, zbw * 512 * 2, depth_data_layout, { zbw, 512, 1 });

	compute_texture_valid = true;
}

void ComputeRenderer::UpdateRenderData() {
	RenderData data{};
	data.scissor_start = scissor_start;
	data.scissor_end = scissor_end;
	data.clut_shift = clut_shift;
	data.clut_mask = clut_mask;
	data.clut_offset = clut_offset;
	data.blend_afix = blend_afix;
	data.blend_bfix = blend_bfix;
	data.alphaMask = alpha_test_mask;
	data.alphaRef = alpha_test_ref & alpha_test_mask;
	data.environment_texture = environment_texture;

	queue.writeBuffer(compute_render_data_buffer, 0, &data, sizeof(RenderData));
	queue.writeBuffer(compute_vertex_buffer, 0, compute_vertices, compute_vertex_buffer_offset);
}

void ComputeRenderer::FlushRender() {
	if (queue_empty) {
		return;
	}

	compute_pass_encoder.end();
	compute_pass_encoder.release();

	auto framebuffer_width = compute_texture.getWidth();
	int bpp = compute_texture.getFormat() == wgpu::TextureFormat::RGBA8Uint ? 4 : 2;
	auto framebuffer_size = 512 * framebuffer_width * bpp;

	wgpu::TexelCopyTextureInfo source{};
	source.texture = compute_texture;

	wgpu::TexelCopyBufferInfo destination{};
	destination.buffer = compute_transitional_buffer;
	destination.layout.bytesPerRow = framebuffer_width * bpp;
	destination.layout.rowsPerImage = 512;

	compute_encoder.copyTextureToBuffer(source, destination, { framebuffer_width, 512, 1 });

	auto depth_buffer_width = compute_depth_texture.getWidth();
	auto depth_buffer_size = 512 * depth_buffer_width * 2;

	wgpu::TexelCopyTextureInfo depth_buffer_source{};
	depth_buffer_source.texture = compute_depth_texture;

	wgpu::TexelCopyBufferInfo depth_buffer_destination{};
	depth_buffer_destination.buffer = compute_depth_transitional_buffer;
	depth_buffer_destination.layout.bytesPerRow = depth_buffer_width * 2;
	depth_buffer_destination.layout.rowsPerImage = 512;

	compute_encoder.copyTextureToBuffer(depth_buffer_source, depth_buffer_destination, { depth_buffer_width, 512, 1 });

	auto command = compute_encoder.finish();
	compute_encoder.release();

	UpdateRenderData();
	queue.submit(command);

	compute_encoder = device.createCommandEncoder();
	compute_pass_encoder = compute_encoder.beginComputePass();

	bool framebuffer_done = false;
	bool depth_buffer_done = false;

	wgpu::BufferMapCallbackInfo map_callback_info{};
	map_callback_info.mode = wgpu::CallbackMode::AllowProcessEvents;
	map_callback_info.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* done, void* _) {
		if (status == wgpu::MapAsyncStatus::Success) {
			*reinterpret_cast<bool*>(done) = true;
		}
	};

	map_callback_info.userdata1 = &framebuffer_done;
	compute_transitional_buffer.mapAsync(wgpu::MapMode::Read, 0, framebuffer_size, map_callback_info);

	map_callback_info.userdata1 = &depth_buffer_done;
	compute_depth_transitional_buffer.mapAsync(wgpu::MapMode::Read, 0, depth_buffer_size, map_callback_info);

	while (!framebuffer_done || !depth_buffer_done) {
		queue.submit(0, nullptr);
	}

	auto framebuffer = PSP::GetInstance()->VirtualToPhysical(0x44000000 | (current_fbp & 0x1FFFF0));
	memcpy(framebuffer, compute_transitional_buffer.getMappedRange(0, framebuffer_size), framebuffer_size);
	compute_transitional_buffer.unmap();

	auto depth_buffer = PSP::GetInstance()->VirtualToPhysical(0x44600000 | (zbp & 0x1FFFF0));
	memcpy(depth_buffer, compute_depth_transitional_buffer.getMappedRange(0, depth_buffer_size), depth_buffer_size);
	compute_depth_transitional_buffer.unmap();

	queue_empty = true;
	compute_vertex_buffer_offset = 0;

	for (auto& entry : deleted_textures) {
		entry.bind_group.release();
		entry.texture.destroy();
	}
	deleted_textures.clear();
}

uint32_t ComputeRenderer::PushVertices(std::vector<Vertex> vertices) {
	auto offset = compute_vertex_buffer_offset;
	for (auto& vertex : vertices) {
		auto compute_vertex = reinterpret_cast<ComputeVertex*>(reinterpret_cast<uintptr_t>(compute_vertices) + compute_vertex_buffer_offset);
		compute_vertex->pos = vertex.pos;
		compute_vertex->uv = vertex.uv;
		compute_vertex->color = glm::uvec4(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
		compute_vertex_buffer_offset += sizeof(ComputeVertex);
	}

	compute_vertex_buffer_offset = ALIGN(compute_vertex_buffer_offset, buffer_alignment);
	return offset;
}

wgpu::BindGroup ComputeRenderer::GetTexture() {
	auto& texture = textures[0];

	auto psp = PSP::GetInstance();
	float bpp{};

	switch (texture_format) {
	case SCEGU_PFIDX4:
		bpp = 0.5;
		break;
	case SCEGU_PFIDX8:
		bpp = 1.0;
		break;
	case SCEGU_PF4444:
		bpp = 2.0;
		break;
	case SCEGU_PF8888:
		bpp = 4.0;
		break;
	default:
		spdlog::error("ComputeRenderer: unknown texture format {}", texture_format);
		return nullptr;
	}

	uint32_t clamped_width = std::min(texture.width, 512u) * bpp;
	uint32_t clamped_height = std::min(texture.height, 512u);

	bool recreate_texture = true;
	TextureCacheEntry cache{};
	if (texture_cache.contains(texture.buffer)) {
		cache = texture_cache[texture.buffer];
		cache.unused_frames = 0;

		if (!cache.dirty) {
			return cache.bind_group;
		}

		if (cache.texture.getWidth() == clamped_width && cache.texture.getHeight() == clamped_height) {
			recreate_texture = false;
		} else {
			deleted_textures.push_back(cache);
		}
	}

	cache.size = texture.pitch * texture.height * bpp;

	uint32_t pitch = texture.pitch * bpp;
	if (recreate_texture) {
		// textures larger than 512x512 will have issues with UV interpolation, let's just hope this doesn't happen
		wgpu::TextureDescriptor texture_desc{};
		texture_desc.dimension = wgpu::TextureDimension::_2D;
		texture_desc.format = wgpu::TextureFormat::R8Uint;
		texture_desc.size = { clamped_width, clamped_height, 1 };
		texture_desc.sampleCount = 1;
		texture_desc.mipLevelCount = 1;
		texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
		cache.texture = device.createTexture(texture_desc);

		wgpu::BindGroupEntry texture_binding{};
		texture_binding.binding = 0;
		texture_binding.textureView = cache.texture.createView();

		wgpu::BindGroupDescriptor texture_bind_group_desc{};
		texture_bind_group_desc.layout = compute_texture_bind_group_layout;
		texture_bind_group_desc.entryCount = 1;
		texture_bind_group_desc.entries = &texture_binding;
		cache.bind_group = device.createBindGroup(texture_bind_group_desc);
	}

	uint32_t* buffer = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(texture.buffer));

	std::vector<uint32_t> unswizzled_texture{};
	if (texture_swizzling) {
		unswizzled_texture.resize(cache.size);

		// Shamelessly stolen from ppsspp
		int bxc = pitch / 16;
		int byc = (texture.height + 7) / 8;

		uint32_t* ydest = unswizzled_texture.data();
		for (int by = 0; by < byc; by++) {
			uint32_t* xdest = ydest;
			for (int bx = 0; bx < bxc; bx++) {
				uint32_t* dest = xdest;
				for (int n = 0; n < 8; n++) {
					memcpy(dest, buffer, 16);
					dest += (pitch >> 2);
					buffer += 4;
				}
				xdest += 4;
			}
			ydest += (pitch >> 2) * 8;
		}
		buffer = unswizzled_texture.data();
	}

	wgpu::TexelCopyTextureInfo destination{};
	destination.texture = cache.texture;

	wgpu::TexelCopyBufferLayout data_layout{};
	data_layout.bytesPerRow = pitch;
	data_layout.rowsPerImage = texture.height;

	queue.writeTexture(destination, buffer, cache.size, data_layout, { std::min(pitch, clamped_width), clamped_height, 1 });

	texture_cache[texture.buffer] = cache;

	return cache.bind_group;
}
