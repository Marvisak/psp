#pragma once

#include <array>
#include <deque>
#include <vector>
#include <chrono>

#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

enum class RendererType {
	SOFTWARE
};

enum ValueFormat {
	FORMAT_NONE = 0,
	FORMAT_BYTE = 1,
	FORMAT_SHORT = 2,
	FORMAT_FLOAT = 3
};

union Color {
	Color() : abgr(0) {}
	Color(uint32_t abgr) : abgr(abgr) {}
	Color(uint8_t a, uint8_t b, uint8_t g, uint8_t r) : a(a), b(b), g(g), r(r) {}

	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	uint32_t abgr;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec2 uv;
	Color color;
};

constexpr auto BASE_WIDTH = 480;
constexpr auto BASE_HEIGHT = 272;
constexpr auto REFRESH_RATE = 60;
constexpr auto FRAME_DURATION = std::chrono::duration<double, std::milli>(1000 / REFRESH_RATE);

struct WaitObject;
struct SyncWaitingThread {
	int thid;
	std::shared_ptr<WaitObject> wait;
};

struct DisplayList {
	uint32_t start_addr;
	uint32_t current_addr;
	uint32_t stall_addr;

	bool valid;
	std::vector<SyncWaitingThread> waiting_threads;
};

struct Texture {
	uint32_t buffer;
	uint32_t pitch;
	uint32_t height;
	uint32_t width;
};

struct Transfer {
	uint32_t buffer;
	uint32_t pitch;
	uint16_t x;
	uint16_t y;
};

class Renderer {
public:
	~Renderer();

	virtual void Frame();
	virtual void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format) = 0;
	virtual void DrawRectangle(Vertex start, Vertex end) = 0;
	virtual void DrawTriangle(Vertex v0, Vertex v1, Vertex v2) = 0;
	virtual void DrawTriangleFan(std::vector<Vertex> vertices) = 0;
	virtual void ClearTextureCache() = 0;
	virtual void ClearTextureCache(uint32_t addr, uint32_t size) = 0;

	void Step();
	bool ShouldRun() { return !waiting && !queue.empty(); }
	int EnQueueList(uint32_t addr, uint32_t stall_addr);
	bool SetStallAddr(int id, uint32_t stall_addr);

	bool SyncThread(int id, int thid);
	void SyncThread(int thid);
	void WakeUp() { waiting = false; }

	uint32_t GetBaseAddress(uint32_t addr) const {
		uint32_t base_addr = ((base & 0xF0000) << 8) | addr;
		return (offset + base_addr) & 0x0FFFFFFF;
	}

	uint32_t GetFrameBufferAddress() const {
		return 0x44000000 | (fbp & 0x1FFFF0);
	}

	uint32_t GetZBufferAddress() const {
		return 0x44600000 | (zbp & 0x1FFFF0);
	}

	int ScissorTestX(int x) const {
		if (x < scissor_start_x) {
			return scissor_start_x;
		} else if (x > scissor_end_x) {
			return scissor_end_x;
		}
		return x;
	}

	int ScissorTestY(int y) const {
		if (y < scissor_start_y) {
			return scissor_start_y;
		}
		else if (y > scissor_end_y) {
			return scissor_end_y;
		}
		return y;
	}

	Vertex ParseVertex();
	void TransformVertex(Vertex& v) const;

	Color ABGR4444ToABGR8888(uint16_t color);
	Color ABGR1555ToABGR8888(uint16_t color);
	Color BGR565ToABGR8888(uint16_t color);

	void Prim(uint32_t opcode);
	void Jump(DisplayList& dl, uint32_t opcode);
	void End(DisplayList& dl, uint32_t opcode);
	void VType(uint32_t opcode);
	void WorldD(uint32_t opcode);
	void ViewD(uint32_t opcode);
	void ProjD(uint32_t opcode);
	void TMode(uint32_t opcode);
	void CLoad(uint32_t opcode);
	void CLUT(uint32_t opcode);
	void TSize(uint32_t opcode);
	void TFunc(uint32_t opcode);
	void FPF(uint32_t opcode);
	void Blend(uint32_t opcode);
	void XStart(uint32_t opcode);
protected:
	Renderer();

	uint32_t offset = 0x0;
	uint32_t base = 0x0;
	uint32_t vaddr = 0x0;

	int world_matrix_num = 0;
	glm::mat4 world_matrix{
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	};
	int view_matrix_num = 0;
	glm::mat4 view_matrix{
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	};
	int projection_matrix_num = 0;
	glm::mat4 projection_matrix{};

	float viewport_scale_y = 0.0;
	float viewport_scale_x = 0.0;
	float viewport_scale_z = 0.0;
	float viewport_pos_x = 0.0;
	float viewport_pos_y = 0.0;
	float viewport_pos_z = 0.0;
	uint32_t viewport_offset_x = 0;
	uint32_t viewport_offset_y = 0;

	bool clear_mode = false;
	bool depth_test = false;
	bool alpha_test = false;

	bool bounding_box_success = false;

	bool gouraud_shading = false;

	Transfer transfer_source{};
	Transfer transfer_dest{};
	uint32_t transfer_width = 0;
	uint32_t transfer_height = 0;

	uint32_t zbp = 0;
	uint16_t zbw = 0;
	uint16_t min_z = 0;
	uint16_t max_z = 65535;
	uint8_t z_test_func = 0;
	bool depth_write = false;

	int scissor_start_x = 0;
	int scissor_start_y = 0;
	int scissor_end_x = 0;
	int scissor_end_y = 0;

	bool blend = false;
	uint8_t blend_operation = 0;
	uint8_t blend_source = 0;
	uint8_t blend_destination = 0;
	glm::ivec4 blend_afix{};
	glm::ivec4 blend_bfix{};

	uint8_t alpha_test_func = 0;
	uint8_t alpha_test_ref = 0;
	uint8_t alpha_test_mask = 0;

	bool material_update = false;
	bool material_diffuse = false;
	bool material_specular = false;

	uint32_t ambient_color = 0x0;
	uint8_t ambient_alpha = 0x0;

	std::array<uint8_t, 1024> clut{};
	uint32_t clut_addr = 0x0;
	uint8_t clut_format = 0;
	uint8_t clut_shift = 0;
	uint8_t clut_mask = 0;
	uint16_t clut_offset = 0;

	std::array<Texture, 8> textures{};
	bool textures_enabled = false;
	bool texture_alpha = false;
	bool fragment_double = false;
	uint8_t texture_format = 0;
	uint8_t texture_function = 0;
	uint8_t texture_magnify_filter = 0;
	uint8_t texture_minify_filter = 0;
	uint8_t texture_level_mode = 0;
	int8_t texture_level_offset = 0;
	glm::ivec4 environment_texture{};

	float u_scale = 0;
	float v_scale = 0;
	float u_offset = 0;
	float v_offset = 0;

	bool culling = false;
	uint8_t cull_type = 0;

	bool through = false;
	uint8_t position_format = FORMAT_NONE;
	uint8_t color_format = FORMAT_NONE;
	uint8_t uv_format = FORMAT_NONE;
	uint8_t weight_format = FORMAT_NONE;
	uint8_t index_format = FORMAT_NONE;
	uint8_t normal_format = FORMAT_NONE;

	uint32_t fbp = 0x0;
	uint32_t fbw = 0x0;
	uint32_t fpf = 0x0;

	int next_id = 1;
	std::deque<int> queue{};
	std::array<DisplayList, 64> display_lists{};
	std::vector<SyncWaitingThread> waiting_threads{};
	bool waiting = false;
	int executed_cycles = 0;

	SDL_Window* window;
	int frames = 0;
	std::chrono::steady_clock::time_point last_frame_time{};
	std::chrono::steady_clock::time_point second_timer{};
};

enum GECommand {
	CMD_NOP = 0x00,
	CMD_VADR = 0x01,
	CMD_IADR = 0x02,
	CMD_PRIM = 0x04,
	CMD_BEZIER = 0x05,
	CMD_SPLINE = 0x06,
	CMD_BBOX = 0x07,
	CMD_JUMP = 0x08,
	CMD_BJUMP = 0x09,
	CMD_CALL = 0x0A,
	CMD_RET = 0x0B,
	CMD_END = 0x0C,
	CMD_SIGNAL = 0x0E,
	CMD_FINISH = 0x0F,
	CMD_BASE = 0x10,
	CMD_VTYPE = 0x12,
	CMD_OFFSET = 0x13,
	CMD_REGION1 = 0x15,
	CMD_REGION2 = 0x16,
	CMD_LTE = 0x17,
	CMD_LE0 = 0x18,
	CMD_LE1 = 0x19,
	CMD_LE2 = 0x1A,
	CMD_LE3 = 0x1B,
	CMD_CLE = 0x1C,
	CMD_BCE = 0x1D,
	CMD_TME = 0x1E,
	CMD_FGE = 0x1F,
	CMD_DTE = 0x20,
	CMD_ABE = 0x21,
	CMD_ATE = 0x22,
	CMD_ZTE = 0x23,
	CMD_STE = 0x24,
	CMD_AAE = 0x25,
	CMD_PCE = 0x26,
	CMD_CTE = 0x27,
	CMD_LOE = 0x28,
	CMD_BONEN = 0x2A,
	CMD_BONED = 0x2B,
	CMD_WEIGHT0 = 0x2C,
	CMD_WEIGHT1 = 0x2D,
	CMD_WEIGHT2 = 0x2E,
	CMD_WEIGHT3 = 0x2F,
	CMD_WEIGHT4 = 0x30,
	CMD_WEIGHT5 = 0x31,
	CMD_WEIGHT6 = 0x32,
	CMD_WEIGHT7 = 0x33,
	CMD_DIVIDE = 0x36,
	CMD_WORLDN = 0x3A,
	CMD_WORLDD = 0x3B,
	CMD_VIEWN = 0x3C,
	CMD_VIEWD = 0x3D,
	CMD_PROJN = 0x3E,
	CMD_PROJD = 0x3F,
	CMD_SX = 0x42,
	CMD_SY = 0x43,
	CMD_SZ = 0x44,
	CMD_TX = 0x45,
	CMD_TY = 0x46,
	CMD_TZ = 0x47,
	CMD_SU = 0x48,
	CMD_SV = 0x49,
	CMD_TU = 0x4A,
	CMD_TV = 0x4B,
	CMD_OFFSETX = 0x4C,
	CMD_OFFSETY = 0x4D,
	CMD_SHADE = 0x50,
	CMD_MATERIAL = 0x53,
	CMD_MAC = 0x55,
	CMD_MAA = 0x58,
	CMD_MK = 0x5B,
	CMD_CULL = 0x9B,
	CMD_FBP = 0x9C,
	CMD_FBW = 0x9D,
	CMD_ZBP = 0x9E,
	CMD_ZBW = 0x9F,
	CMD_TBP0 = 0xA0,
	CMD_TBW0 = 0xA8,
	CMD_CBP = 0xB0,
	CMD_CBW = 0xB1,
	CMD_XBP1 = 0xB2,
	CMD_XBW1 = 0xB3,
	CMD_XBP2 = 0xB4,
	CMD_XBW2 = 0xB5,
	CMD_TSIZE0 = 0xB8,
	CMD_TSIZE1 = 0xB9,
	CMD_TSIZE2 = 0xBA,
	CMD_TSIZE3 = 0xBB,
	CMD_TSIZE4 = 0xBC,
	CMD_TSIZE5 = 0xBD,
	CMD_TSIZE6 = 0xBE,
	CMD_TSIZE7 = 0xBF,
	CMD_TMODE = 0xC2,
	CMD_TPF = 0xC3,
	CMD_CLOAD = 0xC4,
	CMD_CLUT = 0xC5,
	CMD_TFILTER = 0xC6,
	CMD_TLEVEL = 0xC8,
	CMD_TFUNC = 0xC9,
	CMD_TEC = 0xCA,
	CMD_TFLUSH = 0xCB,
	CMD_TSYNC = 0xCC,
	CMD_FPF = 0xD2,
	CMD_CMODE = 0xD3,
	CMD_SCISSOR1 = 0xD4,
	CMD_SCISSOR2 = 0xD5,
	CMD_MINZ = 0xD6,
	CMD_MAXZ = 0xD7,
	CMD_ATEST = 0xDB,
	CMD_ZTEST = 0xDE,
	CMD_BLEND = 0xDF,
	CMD_FIXA = 0xE0,
	CMD_FIXB = 0xE1,
	CMD_DITH1 = 0xE2,
	CMD_DITH2 = 0xE3,
	CMD_DITH3 = 0xE4,
	CMD_DITH4 = 0xE5,
	CMD_ZMSK = 0xE7,
	CMD_PMSK2 = 0xE9,
	CMD_XSTART = 0xEA,
	CMD_XPOS1 = 0xEB,
	CMD_XPOS2 = 0xEC,
	CMD_XSIZE = 0xEE,
};