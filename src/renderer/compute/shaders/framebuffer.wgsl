R"(
struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f
};

struct ScreenTransform {
    scale : vec2f,
    offset : vec2f
}

@group(0) @binding(0) var texture: texture_2d<f32>;
@group(0) @binding(1) var texture_sampler: sampler;
@group(0) @binding(2) var<uniform> frame_width: f32;
@group(0) @binding(3) var<uniform> transform: ScreenTransform;

@group(0) @binding(0) var input_conversion_texture: texture_2d<u32>;
@group(0) @binding(1) var output_conversion_texture: texture_storage_2d<rgba8unorm, write>;

@vertex
fn vs_main(@location(0) pos : vec2f, @location(1) uv_in : vec2f) -> VertexOutput {
    var out : VertexOutput;

    out.position = vec4f(pos, 0.0, 1.0);
    out.uv = uv_in;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var uv = in.uv;
    uv.x = uv.x % (frame_width / 480.0);
    return textureSample(texture, texture_sampler, uv).rgba;
}

@compute @workgroup_size(8, 8)
fn rgba565(@builtin(global_invocation_id) id: vec3<u32>) {
    let dims = textureDimensions(input_conversion_texture);
    if (id.x >= dims.x || id.y >= dims.y) {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let b = f32((pixel >> 11) & 0x1F) / 31.0;
    let g = f32((pixel >> 5)  & 0x3F) / 63.0;
    let r = f32((pixel >> 0)  & 0x1F) / 31.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, 1.0));
}

@compute @workgroup_size(8, 8)
fn rgba5551(@builtin(global_invocation_id) id: vec3<u32>) {
    let dims = textureDimensions(input_conversion_texture);
    if (id.x >= dims.x || id.y >= dims.y) {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let a = f32((pixel >> 15) & 0x01);
    let b = f32((pixel >> 10) & 0x1F) / 31.0;
    let g = f32((pixel >> 5) & 0x1F) / 31.0;
    let r = f32((pixel >> 0) & 0x1F) / 31.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, a));
}

@compute @workgroup_size(8, 8)
fn rgba4444(@builtin(global_invocation_id) id: vec3<u32>) {
    let dims = textureDimensions(input_conversion_texture);
    if (id.x >= dims.x || id.y >= dims.y) {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let a = f32((pixel >> 12) & 0xF) / 15.0;
    let b = f32((pixel >> 8)  & 0xF) / 15.0;
    let g = f32((pixel >> 4)  & 0xF) / 15.0;
    let r = f32((pixel >> 0)  & 0xF) / 15.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, a));
}
)"