R"(
struct Vertex {
    pos: vec4f,
    uv: vec2f,
    color: vec4u
}

struct RenderData {
    scissorStart: vec2i,
    scissorEnd: vec2i,
    clutShift: u32,
    clutMask: u32,
    clutOffset: u32,
    blendAFix: vec4i,
    blendBFix: vec4i,
    alphaMask: u32,
    alphaRef: u32,
    environmentTexture: vec4i
}

override FRAMEBUFFER_FORMAT: u32 = 0;
override TEXTURE_FORMAT: u32 = 100;
override BILINEAR: u32 = 0;
override U_CLAMP: u32 = 0;
override V_CLAMP: u32 = 0;
override CLUT_FORMAT: u32 = 0;
override BLEND_OPERATION: u32 = 100;
override BLEND_SOURCE: u32 = 0;
override BLEND_DESTINATION: u32 = 0;
override ALPHA_FUNC: u32 = 100;
override USE_TEXTURE_ALPHA: u32 = 0;
override FRAGMENT_DOUBLE : u32 = 0;
override TEXTURE_FUNCTION : u32 = 0;
override DEPTH_FUNC: u32 = 100;
override DEPTH_WRITE: u32 = 0;
override GOURAUD_SHADING: u32 = 0;

@group(0) @binding(0) var framebuffer: texture_storage_2d<rgba8uint, read_write>;
@group(0) @binding(1) var framebuffer16: texture_storage_2d<r16uint, read_write>;
@group(0) @binding(2) var depthBuffer: texture_storage_2d<r16uint, read_write>;
@group(1) @binding(0) var<uniform> renderData: RenderData; 
)"