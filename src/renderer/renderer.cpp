#include "renderer.hpp"

#include <format>
#include <thread>
#include <spdlog/spdlog.h>
#include <glm/packing.hpp>

#include "../psp.hpp"
#include "../kernel/thread.hpp"

static void WakeUpRenderer(uint64_t cycles_late) {
	PSP::GetInstance()->GetRenderer()->WakeUp();
}

Renderer::Renderer() {
	window = SDL_CreateWindow("PSP", BASE_WIDTH * 2, BASE_HEIGHT * 2, SDL_WINDOW_RESIZABLE);
	SDL_SetWindowMinimumSize(window, BASE_WIDTH, BASE_HEIGHT);
	second_timer = std::chrono::steady_clock::now();
}

Renderer::~Renderer() {
	SDL_DestroyWindow(window);
}

void Renderer::Frame() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			PSP::GetInstance()->Exit();
			break;
		}
	}

	frames++;
	auto now = std::chrono::steady_clock::now();
	if (now >= second_timer) {
		std::string title = std::format("PSP | {} FPS", frames);
		SDL_SetWindowTitle(window, title.c_str());
		second_timer = now + std::chrono::seconds(1);
		frames = 0;
	}

	auto frame_time = now - last_frame_time;
	if (frame_time < FRAME_DURATION) {
		std::this_thread::sleep_for(FRAME_DURATION - frame_time);
	}
	last_frame_time = std::chrono::steady_clock::now();
}

void Renderer::Step() {
	int current_dl_id = queue.front();
	auto& current_dl = display_lists[current_dl_id];
	if (!current_dl.valid) {
		spdlog::error("Renderer: top display list isn't valid");
		return;
	}

	if (current_dl.stall_addr == current_dl.current_addr) {
		return;
	}

	auto psp = PSP::GetInstance();
	uint32_t command = psp->ReadMemory32(current_dl.current_addr);
	switch (command >> 24) {
	case CMD_NOP: break;
	case CMD_VADR: vaddr = GetBaseAddress(command & 0xFFFFFF); break;
	case CMD_PRIM: Prim(command); break;
	case CMD_JUMP: Jump(current_dl, command); break;
	case CMD_BJUMP: spdlog::warn("BJUMP"); psp->Exit(); break;
	case CMD_CALL: spdlog::warn("CALL"); psp->Exit(); break;
	case CMD_RET: spdlog::warn("RET"); psp->Exit(); break;
	case CMD_END: End(current_dl, command); break;
	case CMD_SIGNAL: spdlog::warn("SIGNAL"); psp->Exit(); break;
	case CMD_FINISH: break;
	case CMD_BASE: base = command; break;
	case CMD_VTYPE: VType(command); break;
	case CMD_OFFSET: offset = command << 8; break;
	case CMD_REGION1: spdlog::warn("Renderer: unimplemented GE command CMD_REGION1"); break;
	case CMD_REGION2: spdlog::warn("Renderer: unimplemented GE command CMD_REGION2"); break;
	case CMD_BCE: culling = command & 1; break;
	case CMD_TME: textures_enabled = command & 1; break;
	case CMD_ABE: blend = command & 1; break;
	case CMD_ATE: alpha_test = command & 1; break;
	case CMD_ZTE: depth_test = command & 1; break;
	case CMD_DIVIDE: spdlog::debug("Renderer: unimplemented GE command CMD_DIVIDE"); break;
	case CMD_WORLDN: world_matrix_num = command & 0xF; break;
	case CMD_WORLDD: WorldD(command); break;
	case CMD_VIEWN: view_matrix_num = command & 0xF; break;
	case CMD_VIEWD: ViewD(command); break;
	case CMD_PROJN: projection_matrix_num = command & 0xF; break;
	case CMD_PROJD: ProjD(command); break;
	case CMD_SX: viewport_scale_x = std::bit_cast<float>(command << 8); break;
	case CMD_SY: viewport_scale_y = std::bit_cast<float>(command << 8); break;
	case CMD_SZ: viewport_scale_z = std::bit_cast<float>(command << 8); break;
	case CMD_TX: viewport_pos_x = std::bit_cast<float>(command << 8); break;
	case CMD_TY: viewport_pos_y = std::bit_cast<float>(command << 8); break;
	case CMD_TZ: viewport_pos_z = std::bit_cast<float>(command << 8); break;
	case CMD_SU: u_scale = std::bit_cast<float>(command << 8); break;
	case CMD_SV: v_scale = std::bit_cast<float>(command << 8); break;
	case CMD_TU: u_offset = std::bit_cast<float>(command << 8); break;
	case CMD_TV: v_offset = std::bit_cast<float>(command << 8); break;
	case CMD_OFFSETX: viewport_offset_x = command & 0xFFFFFF; break;
	case CMD_OFFSETY: viewport_offset_y = command & 0xFFFFFF; break;
	case CMD_SHADE: gouraud_shading = command & 1; break;
	case CMD_MATERIAL: material_update = command & 1; break;
	case CMD_MAC: ambient_color = command & 0xFFFFFF; break;
	case CMD_MAA: ambient_alpha = command & 0xFF; break;
	case CMD_MK: spdlog::warn("Renderer: unimplemented GE command CMD_MK"); break;
	case CMD_CULL: cull_type = command & 0x1; break;
	case CMD_FBP: fbp &= 0xFF000000; fbp |= command & 0xFFFFFF; break;
	case CMD_FBW: fbp &= 0x00FFFFFF; fbp |= (command & 0xFF0000) << 8; fbw = command & 0x07FC; break;
	case CMD_ZBP: zbp &= 0xFF000000; zbp |= command & 0xFFFFFF; break;
	case CMD_ZBW: zbp &= 0x00FFFFFF; zbp |= (command & 0xFF0000) << 8; zbw = command & 0x07FC; break;
	case CMD_TBP0: textures[0].buffer &= 0xFF000000; textures[0].buffer |= command & 0xFFFFFF; break;
	case CMD_TBW0: textures[0].buffer &= 0x00FFFFFF; textures[0].buffer |= (command & 0xFF0000) << 8; textures[0].pitch = command & 0x07FC; break;
	case CMD_CBP: clut_addr &= 0x0F000000; clut_addr |= command & 0xFFFFF0; break;
	case CMD_CBW: clut_addr &= 0x00FFFFFF; clut_addr |= (command & 0xF0000) << 8; break;
	case CMD_XBP1: transfer_source.buffer &= 0xFF000000; transfer_source.buffer |= command & 0xFFFFF0; break;
	case CMD_XBW1: transfer_source.buffer &= 0x00FFFFFF; transfer_source.buffer |= (command & 0xFF0000) << 8; transfer_source.pitch = command & 0x07F8; break;
	case CMD_XBP2: transfer_dest.buffer &= 0xFF000000; transfer_dest.buffer |= command & 0xFFFFF0; break;
	case CMD_XBW2: transfer_dest.buffer &= 0x00FFFFFF; transfer_dest.buffer |= (command & 0xFF0000) << 8; transfer_dest.pitch = command & 0x07F8; break;
	case CMD_TSIZE0:
	case CMD_TSIZE1:
	case CMD_TSIZE2:
	case CMD_TSIZE3:
	case CMD_TSIZE4:
	case CMD_TSIZE5:
	case CMD_TSIZE6:
	case CMD_TSIZE7:
		TSize(command); break;;
	case CMD_TMODE: TMode(command); break;
	case CMD_TPF: texture_format = command & 0xFFFFFF; break;
	case CMD_CLOAD: CLoad(command); break;
	case CMD_CLUT: CLUT(command); break;
	case CMD_TFILTER: texture_minify_filter = command & 0x7; texture_magnify_filter = (command >> 8) & 0x7; break;
	case CMD_TWRAP: u_clamp = (command & 1) != 0; v_clamp = (command & 0x100) != 0; break;
	case CMD_TLEVEL: texture_level_mode = command & 0x3; texture_level_offset = static_cast<int8_t>((command >> 16) & 0xFF); break;
	case CMD_TFUNC: TFunc(command); break;
	case CMD_TEC: environment_texture = { command & 0xFF, command >> 8 & 0xFF, command >> 16 & 0xFF, 0x00 }; break;
	case CMD_TFLUSH: break;
	case CMD_TSYNC: break;
	case CMD_FPF: FPF(command); break;
	case CMD_CMODE: clear_mode = command & 1; break;
	case CMD_SCISSOR1: scissor_start_x = command & 0x1FF; scissor_start_y = command >> 10 & 0x1FF; break;
	case CMD_SCISSOR2: scissor_end_x = command & 0x1FF; scissor_end_y = command >> 10 & 0x1FF; break;
	case CMD_MINZ: min_z = command & 0xFFFF; break;
	case CMD_MAXZ: max_z = command & 0xFFFF; break;
	case CMD_ATEST: alpha_test_func = command & 0x7; alpha_test_ref = (command >> 8) & 0xFF; alpha_test_mask = (command >> 16) & 0xFF; break;
	case CMD_ZTEST: z_test_func = command & 0x7; break;
	case CMD_BLEND: Blend(command); break;
	case CMD_FIXA: blend_afix = { command & 0xFF, command >> 8 & 0xFF, command >> 16 & 0xFF, 0x00 }; break;
	case CMD_FIXB: blend_bfix = { command & 0xFF, command >> 8 & 0xFF, command >> 16 & 0xFF, 0x00 }; break;
	case CMD_DITH1: spdlog::warn("Renderer: unimplemented GE command CMD_DITH1"); break;
	case CMD_DITH2: spdlog::warn("Renderer: unimplemented GE command CMD_DITH2"); break;
	case CMD_DITH3: spdlog::warn("Renderer: unimplemented GE command CMD_DITH3"); break;
	case CMD_DITH4: spdlog::warn("Renderer: unimplemented GE command CMD_DITH4"); break;
	case CMD_ZMSK: depth_write = !(command & 1); break;
	case CMD_PMSK2: spdlog::warn("Renderer: unimplemented GE command CMD_PMSK2"); break;
	case CMD_XSTART: XStart(command); break;
	case CMD_XPOS1: transfer_source.x = command & 0x3FF; transfer_source.y = (command >> 10) & 0x3FF; break;
	case CMD_XPOS2: transfer_dest.x = command & 0x3FF; transfer_dest.y = (command >> 10) & 0x3FF; break;
	case CMD_XSIZE: transfer_width = command & 0x3FF; transfer_height = (command >> 10) & 0x3FF; break;
	default:
		spdlog::error("Renderer: unknown GE command {:x}", command >> 24);
		break;
	}
	current_dl.current_addr += 4;

	if (executed_cycles) {
		psp->Schedule(executed_cycles, WakeUpRenderer);
		waiting = true;
		executed_cycles = 0;
	}

	if (queue.empty()) {
		for (auto& thread : waiting_threads) {
			thread.wait->ended = true;
			psp->GetKernel()->WakeUpThread(thread.thid);
		}
		waiting_threads.clear();
	}
}

int Renderer::EnQueueList(uint32_t addr, uint32_t stall_addr) {
	DisplayList dl{};
	dl.start_addr = addr;
	dl.current_addr = addr & 0x0FFFFFFF;
	dl.stall_addr = stall_addr & 0x0FFFFFFF;
	dl.valid = true;

	for (int i = next_id; i < display_lists.size(); i++) {
		if (!display_lists[i].valid) {
			display_lists[i] = dl;
			++next_id %= display_lists.size();
			if (next_id == 0) next_id++;
			queue.push_back(i);
			return i;
		}
	}

	return -1;
}

bool Renderer::SetStallAddr(int id, uint32_t addr) {
	if (id < 0 && id > display_lists.size()) {
		return false;
	}
	auto& display_list = display_lists[id];
	if (display_list.valid) {
		display_list.stall_addr = addr & 0x0FFFFFFF;
	}
	return display_list.valid;
}

bool Renderer::SyncThread(int id, int thid) {
	if (id < 0 && id > display_lists.size()) {
		return false;
	}
	auto& display_list = display_lists[id];
	if (display_list.valid) {
		SyncWaitingThread thread{};
		thread.thid = thid;
		thread.wait = PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::LIST_SYNC, false);
		display_list.waiting_threads.push_back(thread);
	}
	return display_list.valid;
}

void Renderer::SyncThread(int thid) {
	if (!queue.empty()) {
		SyncWaitingThread thread{};
		thread.thid = thid;
		thread.wait = PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::DRAW_SYNC, false);
		waiting_threads.push_back(thread);
	}
}

void Renderer::TransformVertex(Vertex& v) const {
	glm::vec4 model_pos(v.pos, 1.0);
	glm::mat4 worldviewproj = world_matrix * view_matrix * projection_matrix;
	glm::vec4 clip_pos = model_pos * worldviewproj;

	float x = clip_pos.x * viewport_scale_x / clip_pos.w + viewport_pos_x;
	float y = clip_pos.y * viewport_scale_y / clip_pos.w + viewport_pos_y;
	float z = clip_pos.z * viewport_scale_z / clip_pos.w + viewport_pos_z;

	v.pos.x = (x * 16 - (viewport_offset_x & 0xFFFF)) * (1.0f / 16.0f);
	v.pos.y = (y * 16 - (viewport_offset_y & 0xFFFF)) * (1.0f / 16.0f);
	v.pos.z = z;
}

Vertex Renderer::ParseVertex() {
	auto psp = PSP::GetInstance();

	Vertex v{};

	if (index_format != FORMAT_NONE) {
		spdlog::error("Renderer: index not supported");
	}

	if (uv_format != FORMAT_NONE) {
		switch (uv_format) {
		case FORMAT_BYTE:
			v.uv.x = psp->ReadMemory8(vaddr) * (1.f / 128.f);
			v.uv.y = psp->ReadMemory8(vaddr + 1) * (1.f / 128.f);
			vaddr += 2;
			break;
		case FORMAT_SHORT:
			vaddr = ALIGN(vaddr, 2);
			v.uv.x = static_cast<float>(psp->ReadMemory16(vaddr));
			v.uv.y = static_cast<float>(psp->ReadMemory16(vaddr + 2));
			if (!through) {
				v.uv.x *= (1.f / 32768.f) * u_scale;
				v.uv.y *= (1.f / 32768.f) * v_scale;
				v.uv.x += u_offset;
				v.uv.y += v_offset;
			}
			vaddr += 4;
			break;
		case FORMAT_FLOAT:
			vaddr = ALIGN(vaddr, 4);
			v.uv.x = std::bit_cast<float>(psp->ReadMemory32(vaddr));
			v.uv.y = std::bit_cast<float>(psp->ReadMemory32(vaddr + 4));
			vaddr += 8;
			break;
		}

		if (!through) {
			v.uv.x *= textures[0].width;
			v.uv.y *= textures[0].height;
		}
		v.uv = glm::round(v.uv);
		v.uv.x /= textures[0].width;
		v.uv.y /= textures[0].height;
	}

	switch (color_format) {
	case FORMAT_NONE:
		v.color = (ambient_alpha << 24) | ambient_color;
		break;
	case SCEGU_COLOR_PF5650:
		vaddr = ALIGN(vaddr, 2);
		v.color = BGR565ToABGR8888(psp->ReadMemory16(vaddr));
		vaddr += 2;
		break;
	case SCEGU_COLOR_PF5551:
		vaddr = ALIGN(vaddr, 2);
		v.color = ABGR1555ToABGR8888(psp->ReadMemory16(vaddr));
		vaddr += 2;
		break;
	case SCEGU_COLOR_PF4444:
		vaddr = ALIGN(vaddr, 2);
		v.color = ABGR4444ToABGR8888(psp->ReadMemory16(vaddr));
		vaddr += 2;
		break;
	case SCEGU_COLOR_PF8888:
		vaddr = ALIGN(vaddr, 4);
		v.color.abgr = psp->ReadMemory32(vaddr);
		vaddr += 4;
		break;
	}

	if (normal_format != FORMAT_NONE) {
		spdlog::error("Renderer: normal not supported");
		switch (normal_format) {
		case FORMAT_BYTE:
			vaddr++;
			break;
		case FORMAT_SHORT:
			vaddr += 2;
			break;
		case FORMAT_FLOAT:
			vaddr += 4;
			break;
		}
	}

	switch (position_format) {
	case FORMAT_BYTE:
		vaddr = ALIGN(vaddr, 1);
		if (through) {
			v.pos = glm::vec3(0);
		} else {
			v.pos.x = static_cast<int8_t>(psp->ReadMemory8(vaddr));
			v.pos.y = static_cast<int8_t>(psp->ReadMemory8(vaddr + 1));
			v.pos.z = static_cast<int8_t>(psp->ReadMemory8(vaddr + 2));
			v.pos *= 1.0f / 128.0f;
		}
		vaddr += 3;
		break;
	case FORMAT_SHORT:
		vaddr = ALIGN(vaddr, 2);
		v.pos.x = static_cast<int16_t>(psp->ReadMemory16(vaddr));
		v.pos.y = static_cast<int16_t>(psp->ReadMemory16(vaddr + 2));
		v.pos.z = static_cast<uint16_t>(psp->ReadMemory16(vaddr + 4));
		if (!through) {
			v.pos *= 1.0f / 32768.0f;
		}
		vaddr += 6;
		break;
	case FORMAT_FLOAT:
		vaddr = ALIGN(vaddr, 4);
		v.pos.x = std::bit_cast<float>(psp->ReadMemory32(vaddr));
		v.pos.y = std::bit_cast<float>(psp->ReadMemory32(vaddr + 4));
		v.pos.z = std::bit_cast<float>(psp->ReadMemory32(vaddr + 8));
		vaddr += 12;
		break;
	}

	if (!through) {
		TransformVertex(v);
	}

	return v;
}

Color Renderer::ABGR1555ToABGR8888(uint16_t color) {
	uint8_t a = (color >> 15) & 0x1;
	uint8_t b = (color >> 10) & 0x1F;
	uint8_t g = (color >> 5) & 0x1F;
	uint8_t r = color & 0x1F;

	b = (b << 3) | (b >> 2);
	g = (g << 3) | (g >> 2);
	r = (r << 3) | (r >> 2);

	a = a ? 0xFF : 0x00;

	return { a, b, g, r };
}

Color Renderer::ABGR4444ToABGR8888(uint16_t color) {
	uint8_t a = (color >> 12) & 0xF;
	uint8_t b = (color >> 8) & 0xF;
	uint8_t g = (color >> 4) & 0xF;
	uint8_t r = color & 0xF;

	a = (a << 4) | a;
	b = (b << 4) | b;
	g = (g << 4) | g;
	r = (r << 4) | r;

	return { a, b, g, r };
}

Color Renderer::BGR565ToABGR8888(uint16_t color) {
	uint8_t b = (color >> 11) & 0x1F;
	uint8_t g = (color >> 5) & 0x3F;
	uint8_t r = color & 0x1F;

	b = (b << 3) | (b >> 2);
	g = (g << 2) | (g >> 4);
	r = (r << 3) | (r >> 2);

	return { 0xFF, b, g, r };
}

void Renderer::Prim(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	uint8_t primitive_type = opcode >> 16 & 7;
	uint16_t count = opcode & 0xFFFF;

	if (position_format == FORMAT_NONE) {
		spdlog::error("Renderer: invalid position format FORMAT_NONE");
		return;
	}

	std::vector<Vertex> vertices;
	for (int i = 0; i < count; i++) {
		vertices.push_back(ParseVertex());
	}

	executed_cycles = count * 40;

	switch (primitive_type) {
	case SCEGU_PRIM_TRIANGLES: 
		for (int i = 0; i < vertices.size() - 2; i += 3) {
			DrawTriangle(vertices[i], vertices[i + 1], vertices[i + 2]);
		}
		break;
	case SCEGU_PRIM_TRIANGLE_STRIP: {
		if (count >= 3) {
			DrawTriangleStrip(vertices);
		}
		break;
	}
	case SCEGU_PRIM_TRIANGLE_FAN: 
		if (count >= 3) {
			DrawTriangleFan(vertices);
		}
		break;
	case SCEGU_PRIM_RECTANGLES:
		for (int i = 0; i < vertices.size() - 1; i += 2) {
			DrawRectangle(vertices[i], vertices[i + 1]);
		}
		break;
	default:
		spdlog::error("Renderer: unknown primitive type {}", primitive_type);
	}
}

void Renderer::End(DisplayList& dl, uint32_t opcode) {
	executed_cycles += 60;
	dl.valid = false;
	queue.pop_front();
	for (auto& thread : dl.waiting_threads) {
		thread.wait->ended = true;
		PSP::GetInstance()->GetKernel()->WakeUpThread(thread.thid);
	}
	dl.waiting_threads.clear();
}

void Renderer::Jump(DisplayList& dl, uint32_t opcode) {
	uint32_t current = dl.current_addr;
	dl.current_addr = GetBaseAddress(opcode & 0xFFFFFC) - 4;
	executed_cycles += (dl.current_addr - current) / 2;
}

void Renderer::CLoad(uint32_t opcode) {
	uint32_t total_bytes = std::min((opcode & 0x3F) * 32, 1024U);

	auto psp = PSP::GetInstance();
	void* addr = psp->VirtualToPhysical(clut_addr);
	if (addr) {
		uint32_t valid_size = psp->GetMaxSize(clut_addr);
		if (valid_size < total_bytes) {
			memcpy(clut.data(), addr, valid_size);
			memset(clut.data() + valid_size, 0, total_bytes - valid_size);
		}
		else {
			memcpy(clut.data(), addr, total_bytes);
		}
	}
	else {
		memset(clut.data(), 0, total_bytes);
	}
}

void Renderer::CLUT(uint32_t opcode) {
	clut_format = opcode & 3;
	clut_shift = (opcode >> 2) & 0x1F;
	clut_mask = (opcode >> 8) & 0xFF;
	clut_offset = ((opcode >> 16) & 0x1F) << 4;
}

void Renderer::TSize(uint32_t opcode) {
	auto& texture = textures[(opcode >> 24) - CMD_TSIZE0];
	texture.width = 1 << (opcode & 0xF);
	texture.height = 1 << ((opcode >> 8) & 0xF);
}

void Renderer::TMode(uint32_t opcode) {
	if ((opcode & 1) != 0) {
		spdlog::error("Renderer: texture swizzling not implemented");
		return;
	}
}

void Renderer::TFunc(uint32_t opcode) {
	fragment_double = opcode & 0x10000;
	texture_alpha = (opcode & 0x100) != 0;
	texture_function = opcode & 7;
}

void Renderer::VType(uint32_t opcode) {
	through = (opcode & 0x800000) != 0;
	index_format = opcode >> 11 & 3;
	weight_format = opcode >> 9 & 3;
	position_format = opcode >> 7 & 3;
	normal_format = opcode >> 5 & 3;
	color_format = opcode >> 2 & 7;
	uv_format = opcode & 3;
}

void Renderer::WorldD(uint32_t opcode) {
	if (world_matrix_num < 12) {
		world_matrix[world_matrix_num % 3][world_matrix_num / 3] = std::bit_cast<float>(opcode << 8);
	}
	world_matrix_num++;
}

void Renderer::ViewD(uint32_t opcode) {
	if (view_matrix_num < 12) {
		view_matrix[view_matrix_num % 3][view_matrix_num / 3] = std::bit_cast<float>(opcode << 8);
	}
	view_matrix_num++;
}

void Renderer::ProjD(uint32_t opcode) {
	if (projection_matrix_num < 16) {
		projection_matrix[projection_matrix_num % 4][projection_matrix_num / 4] = std::bit_cast<float>(opcode << 8);
	}
	projection_matrix_num++;
}

void Renderer::FPF(uint32_t opcode) {
	uint32_t format = opcode & 0xFFFFFF;
	if (format > 3) {
		spdlog::warn("Renderer: unknown framebuffer pixel format passed: {}", format);
	} else {
		fpf = format;
	}
}
void Renderer::Blend(uint32_t opcode) {
	blend_source = opcode & 0xF;
	blend_destination = opcode >> 4 & 0xF;
	blend_operation = opcode >> 8 & 0xF;
}

void Renderer::XStart(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	auto src = psp->VirtualToPhysical(transfer_source.buffer);
	auto dest = psp->VirtualToPhysical(transfer_dest.buffer);

	int bpp = 0;
	for (int y = 0; y <= transfer_height; y++) {
		uint32_t src_index = (transfer_source.y + y) * transfer_source.pitch + transfer_source.x;
		uint32_t dest_index = (transfer_dest.y + y) * transfer_dest.pitch + transfer_dest.x;
		if (opcode & 1) {
			bpp = 4;
			memcpy(reinterpret_cast<uint32_t*>(dest) + dest_index, reinterpret_cast<uint32_t*>(src) + src_index, (transfer_width + 1) * 4);
		} else {
			bpp = 2;
			memcpy(reinterpret_cast<uint16_t*>(dest) + dest_index, reinterpret_cast<uint16_t*>(src) + src_index, (transfer_width + 1) * 2);
		}
	}

	executed_cycles = ((transfer_height + 1) * (transfer_width + 1) * bpp * 16) / 10;
}