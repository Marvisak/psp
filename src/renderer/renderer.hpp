#pragma once

#include <array>
#include <deque>
#include <vector>
#include <chrono>

#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "..\hle\defs.hpp"

constexpr auto TEXTURE_CACHE_CLEAR_FRAMES = 120;

enum class RendererType {
	SOFTWARE,
	COMPUTE
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
	glm::vec4 pos;
	glm::vec2 uv;
	Color color;
};

constexpr auto BASE_WIDTH = 480;
constexpr auto BASE_HEIGHT = 272;
constexpr auto BASE_WINDOW_WIDTH = BASE_WIDTH * 2;
constexpr auto BASE_WINDOW_HEIGHT = BASE_HEIGHT * 2;
constexpr auto REFRESH_RATE = 59.9400599f;
constexpr auto FRAME_DURATION = std::chrono::duration<double, std::milli>(1000.f / REFRESH_RATE);

struct WaitObject;
struct SyncWaitingThread {
	int thid;
	std::shared_ptr<WaitObject> wait;
};

struct DisplayListStackEntry {
	uint32_t addr;
	uint32_t offset_addr;
	uint32_t base_addr;
};

struct DisplayList {
	uint32_t start_addr;
	uint32_t current_addr;
	uint32_t context_addr;
	uint32_t offset_addr;

	DisplayListStackEntry stack[32];
	int stack_ptr;

	uint32_t stall_addr;

	bool bounding_box_check;

	int cbid;
	int signal;
	int state;
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
	glm::uvec2 pos;
};

class Renderer {
public:
	virtual ~Renderer();

	virtual void Frame();
	virtual void SetFrameBuffer(uint32_t frame_buffer, int frame_width, int pixel_format) { flips++; }
	virtual void Resize(int width, int height) = 0;
	virtual void RenderFramebufferChange() = 0;
	virtual void DrawPoint(Vertex point) = 0;
	virtual void DrawLine(Vertex start, Vertex end) = 0;
	virtual void DrawLineStrip(std::vector<Vertex> vertices) = 0;
	virtual void DrawRectangle(Vertex start, Vertex end) = 0;
	virtual void DrawTriangle(Vertex v0, Vertex v1, Vertex v2) = 0;
	virtual void DrawTriangleStrip(std::vector<Vertex> vertices) = 0;
	virtual void DrawTriangleFan(std::vector<Vertex> vertices) = 0;
	virtual void ClearTextureCache() = 0;
	virtual void ClearTextureCache(uint32_t addr, uint32_t size) = 0;
	virtual void FlushRender() = 0;

	void Run();
	void ExecuteCommand(uint32_t command);
	int EnQueueList(uint32_t addr, uint32_t stall_addr, int cbid, uint32_t opt, bool head);
	void DeQueueList(int id);
	int SetStallAddr(int id, uint32_t stall_addr);
	int Break(int mode);
	int Continue();
	int GetStack(int index, uint32_t stack_addr);
	uint32_t Get(int cmd) { return cmds[cmd]; }

	bool IsBusy();
	int GetStatus(int id);
	int GetStatus();
	int SyncThread(int id, int thid);
	void SyncThread(int thid);
	void HandleListSync(DisplayList& dl);
	void HandleDrawSync();

	void SaveContext(uint32_t* context);
	void RestoreContext(uint32_t* context);

	uint32_t GetBaseAddress(uint32_t addr) const {
		uint32_t base_addr = ((base & 0xF0000) << 8) | addr;
		return (offset + base_addr) & 0x0FFFFFFF;
	}

	uint32_t GetFrameBufferAddress() const {
		return 0x44000000 | (fbp & 0x1FFFF0);
	}

	uint32_t GetDepthBufferAddress() const {
		return 0x44600000 | (zbp & 0x1FFFF0);
	}

	int ScissorTestX(int x) const {
		if (x < scissor_start.x) {
			return scissor_start.x;
		} else if (x > scissor_end.x) {
			return scissor_end.x;
		}
		return x;
	}

	int ScissorTestY(int y) const {
		if (y < scissor_start.y) {
			return scissor_start.y;
		}
		else if (y > scissor_end.y) {
			return scissor_end.y;
		}
		return y;
	}

	Vertex ParseVertex();
	bool TransformVertex(Vertex& v) const;

	uint8_t GetFilter(float du, float dv);

	Color ABGR4444ToABGR8888(uint16_t color);
	Color ABGR1555ToABGR8888(uint16_t color);
	Color BGR565ToABGR8888(uint16_t color);

	void Prim(uint32_t opcode);
	void BBox(uint32_t opcode);
	void Jump(uint32_t opcode);
	void BJump(uint32_t opcode);
	void Call(uint32_t opcode);
	void Ret(uint32_t opcode);
	void End(uint32_t opcode);
	void VType(uint32_t opcode);
	void WorldD(uint32_t opcode);
	void ViewD(uint32_t opcode);
	void ProjD(uint32_t opcode);
	virtual void CLoad(uint32_t opcode);
	void CLUT(uint32_t opcode);
	void TSize(uint32_t opcode);
	void TFunc(uint32_t opcode);
	void Blend(uint32_t opcode);
	void XStart(uint32_t opcode);
protected:
	Renderer();

	std::array<uint32_t, 512> cmds{};

	uint32_t offset = 0x0;
	uint32_t base = 0x0;
	uint32_t vaddr = 0x0;
	uint32_t iaddr = 0x0;

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
	int bone_matrix_num = 0;
	glm::mat4 bone_matrix{
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	};
	int texture_matrix_num = 0;
	glm::mat4 texture_matrix{
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	};

	glm::ivec2 min_draw_area{};
	glm::ivec2 max_draw_area{};

	glm::vec3 viewport_scale{};
	glm::vec3 viewport_pos{};
	glm::uvec2 viewport_offset{};

	bool clear_mode = false;
	bool clear_mode_depth = false;

	bool depth_test = false;
	bool alpha_test = false;

	bool clip_plane = false;

	bool gouraud_shading = false;

	Transfer transfer_source{};
	Transfer transfer_dest{};
	glm::uvec2 transfer_size{};

	uint32_t zbp = 0;
	uint16_t zbw = 0;
	int16_t min_z = 0;
	int16_t max_z = 65535;
	uint8_t depth_test_func = 0;
	bool depth_write = false;

	glm::ivec2 scissor_start{};
	glm::ivec2 scissor_end{};

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
	bool texture_swizzling = false;
	bool fragment_double = false;
	uint8_t texture_format = 0;
	uint8_t texture_function = 0;
	uint8_t texture_magnify_filter = 0;
	uint8_t texture_minify_filter = 0;
	uint8_t texture_level_mode = 0;
	int8_t texture_level_offset = 0;
	bool u_clamp = false;
	bool v_clamp = false;
	glm::ivec4 environment_texture{};

	glm::vec2 uv_scale{};
	glm::vec2 uv_offset{};

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

	int next_id = 0;
	std::deque<int> queue{};
	std::array<DisplayList, 64> display_lists{};
	std::vector<SyncWaitingThread> waiting_threads{};
	int executed_cycles = 0;

	SDL_Window* window;
	bool frame_limiter = true;
	int frames = 0;
	int flips = 0;
	std::chrono::steady_clock::time_point second_timer{};
	std::chrono::steady_clock::time_point last_frame_time{};
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
	CMD_ORIGIN = 0x14,
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
	CMD_MDC = 0x56,
	CMD_MSC = 0x57,
	CMD_MAA = 0x58,
	CMD_MK = 0x5B,
	CMD_AC = 0x5C,
	CMD_AA = 0x5D,
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
	CMD_TWRAP = 0xC7,
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