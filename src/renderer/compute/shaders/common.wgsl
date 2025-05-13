R"(
struct Vertex {
    pos: vec4f,
    uv: vec2f,
    color: vec4u
}

struct RenderData {
    scissorStart: vec2u,
    scissorEnd: vec2u,
    clutShift: u32,
    clutMask: u32,
    clutOffset: u32,
    blendAFix: vec4i,
    blendBFix: vec4i,
}

override TEXTURES_ENABLED: u32 = 0;
override TEXTURE_FORMAT: u32 = 0;
override U_CLAMP: u32 = 0;
override V_CLAMP: u32 = 0;
override CLUT_FORMAT: u32 = 100;
override BLEND_OPERATION: u32 = 100;
override BLEND_SOURCE: u32 = 0;
override BLEND_DESTINATION: u32 = 0;

@group(0) @binding(0) var framebuffer: texture_storage_2d<rgba8uint, read_write>;
@group(1) @binding(0) var<uniform> renderData: RenderData; 
)"