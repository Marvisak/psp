#define WEBGPU_CPP_IMPLEMENTATION

#include "renderer.hpp"

#include <spdlog/spdlog.h>
#include <sdl3webgpu.h>

#include "../../psp.hpp"
#include "../../hle/defs.hpp"
#include "shaders/shaders.h"

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

ComputeRenderer::ComputeRenderer() : Renderer() {
	wgpu::InstanceDescriptor desc{};
	instance = wgpu::createInstance(desc);

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
		spdlog::info("Using {}", std::string(adapter_info.device.data, adapter_info.device.length));
	}

	wgpu::DeviceDescriptor device_desc{};
	device = adapter.requestDevice(device_desc);

	queue = device.getQueue();

	wgpu::BufferDescriptor vertex_buffer_desc{};
	vertex_buffer_desc.size = sizeof(QUAD_VERTICES);
	vertex_buffer_desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
	vertex_buffer = device.createBuffer(vertex_buffer_desc);
	queue.writeBuffer(vertex_buffer, 0, QUAD_VERTICES, sizeof(QUAD_VERTICES));

	wgpu::BufferDescriptor index_buffer_desc{};
	index_buffer_desc.size = sizeof(QUAD_INDICES);
	index_buffer_desc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
	index_buffer = device.createBuffer(index_buffer_desc);
	queue.writeBuffer(index_buffer, 0, QUAD_INDICES, sizeof(QUAD_INDICES));

	wgpu::ShaderSourceWGSL shader_source{};
	shader_source.chain.sType = wgpu::SType::ShaderSourceWGSL;
	shader_source.code = WGPUStringView(framebuffer_shader, WGPU_STRLEN);

	wgpu::ShaderModuleDescriptor shader_desc{};
	shader_desc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&shader_source);

	auto shader_module = device.createShaderModule(shader_desc);

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
	fragment_state.entryPoint = WGPUStringView("fs_main", WGPU_STRLEN);
	fragment_state.targetCount = 1;
	fragment_state.targets = &color_target;

	wgpu::VertexState vertex_state{};
	vertex_state.module = shader_module;
	vertex_state.entryPoint = WGPUStringView("vs_main", WGPU_STRLEN);
	vertex_state.bufferCount = 1;
	vertex_state.buffers = &vertex_layout;

	wgpu::BindGroupLayoutEntry render_binding_layouts[4]{};

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

	wgpu::TextureDescriptor texture_desc{};
	texture_desc.dimension = wgpu::TextureDimension::_2D;
	texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
	texture_desc.size = { BASE_WIDTH, BASE_HEIGHT, 1 };
	texture_desc.sampleCount = 1;
	texture_desc.mipLevelCount = 1;
	texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::StorageBinding;
	framebuffer_texture = device.createTexture(texture_desc);

	wgpu::TextureViewDescriptor texture_view_desc{};
	texture_view_desc.aspect = wgpu::TextureAspect::All;
	texture_view_desc.arrayLayerCount = 1;
	texture_view_desc.mipLevelCount = 1;
	texture_view_desc.dimension = wgpu::TextureViewDimension::_2D;
	texture_view_desc.format = wgpu::TextureFormat::RGBA8Unorm;
	auto texture_view = framebuffer_texture.createView(texture_view_desc);

	wgpu::SamplerDescriptor sampler_desc{};
	sampler_desc.addressModeU = wgpu::AddressMode::Repeat;
	sampler_desc.addressModeV = wgpu::AddressMode::Repeat;
	sampler_desc.magFilter = wgpu::FilterMode::Nearest;
	sampler_desc.minFilter = wgpu::FilterMode::Nearest;
	sampler_desc.maxAnisotropy = 1;
	nearest_sampler = device.createSampler(sampler_desc);

	wgpu::BufferDescriptor frame_width_buffer_desc{};
	frame_width_buffer_desc.size = sizeof(float);
	frame_width_buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	frame_width_buffer = device.createBuffer(frame_width_buffer_desc);

	wgpu::BindGroupEntry render_bindings[4];
	render_bindings[0].binding = 0;
	render_bindings[0].textureView = texture_view;
	
	render_bindings[1].binding = 1;
	render_bindings[1].sampler = nearest_sampler;

	render_bindings[2].binding = 2;
	render_bindings[2].buffer = frame_width_buffer;
	render_bindings[2].size = sizeof(float);

	wgpu::BindGroupDescriptor bind_group_desc{};
	bind_group_desc.layout = render_bind_group_layout;
	bind_group_desc.entryCount = 3;
	bind_group_desc.entries = render_bindings;
	render_bind_group = device.createBindGroup(bind_group_desc);

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
		"rgba565",
		"rgba5551",
		"rgba4444"
	};

	wgpu::PipelineLayoutDescriptor framebuffer_conversion_layout_desc{};
	framebuffer_conversion_layout_desc.bindGroupLayoutCount = 1;
	framebuffer_conversion_layout_desc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&framebuffer_conversion_bind_group_layout);
	framebuffer_conversion_layout = device.createPipelineLayout(framebuffer_conversion_layout_desc);

	for (int i = SCE_DISPLAY_PIXEL_RGB565; i < SCE_DISPLAY_PIXEL_RGBA8888; i++) {
		wgpu::ComputePipelineDescriptor compute_pipeline_desc{};
		compute_pipeline_desc.compute.entryPoint = WGPUStringView(compute_entrypoints[i], WGPU_STRLEN);
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

	Resize(BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT);
}

ComputeRenderer::~ComputeRenderer() {
	for (int i = SCE_DISPLAY_PIXEL_RGB565; i < SCE_DISPLAY_PIXEL_RGBA8888; i++) {
		framebuffer_conversion_pipelines[i].release();
	}

	framebuffer_conversion_texture.release();
	framebuffer_conversion_bind_group.release();
	framebuffer_conversion_layout.release();
	framebuffer_conversion_bind_group_layout.release();
	frame_width_buffer.release();
	framebuffer_texture.release();
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
	this->frame_buffer = frame_buffer;
	this->pixel_format = pixel_format;

	if (this->frame_width != frame_width) {
		this->frame_width = frame_width;

		float buffer_data = frame_width;
		queue.writeBuffer(frame_width_buffer, 0, &buffer_data, sizeof(float));
	}
}
