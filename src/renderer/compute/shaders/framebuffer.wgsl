R"(
struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f
};

@group(0) @binding(0) var texture: texture_2d<f32>;
@group(0) @binding(1) var texture_sampler: sampler;
@group(0) @binding(2) var<uniform> frame_width: f32;

@group(0) @binding(0) var input_conversion_texture: texture_2d<u32>;
@group(0) @binding(1) var output_conversion_texture: texture_storage_2d<rgba8unorm, write>;

@vertex
fn vsMain(@location(0) pos : vec2f, @location(1) uv_in : vec2f) -> VertexOutput {
    var out : VertexOutput;

    out.position = vec4f(pos, 0.0, 1.0);
    out.uv = uv_in;

    return out;
}

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    var uv = in.uv;
    uv.x = uv.x % (frame_width / 480.0);
    return textureSample(texture, texture_sampler, uv).rgba;
}

@compute @workgroup_size(8, 8)
fn rgb565(@builtin(global_invocation_id) id: vec3u) {
    let dims = textureDimensions(input_conversion_texture);
    if id.x >= dims.x || id.y >= dims.y {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let b = f32(extractBits(pixel, 11u, 5u)) / 31.0;
    let g = f32(extractBits(pixel, 5u, 6u)) / 63.0;
    let r = f32(extractBits(pixel, 0u, 5u)) / 31.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, 1.0));
}

@compute @workgroup_size(8, 8)
fn rgba5551(@builtin(global_invocation_id) id: vec3u) {
    let dims = textureDimensions(input_conversion_texture);
    if id.x >= dims.x || id.y >= dims.y {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let a = f32(extractBits(pixel, 15u, 1u));
    let b = f32(extractBits(pixel, 10u, 5u)) / 31.0;
    let g = f32(extractBits(pixel, 5u, 5u)) / 31.0;
    let r = f32(extractBits(pixel, 0u, 5u)) / 31.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, a));
}

@compute @workgroup_size(8, 8)
fn rgba4444(@builtin(global_invocation_id) id: vec3u) {
    let dims = textureDimensions(input_conversion_texture);
    if id.x >= dims.x || id.y >= dims.y {
        return;
    }

    let pixel = textureLoad(input_conversion_texture, id.xy, 0).r;

    let a = f32(extractBits(pixel, 12u, 4u)) / 15.0;
    let b = f32(extractBits(pixel, 8u, 4u))  / 15.0;
    let g = f32(extractBits(pixel, 4u, 4u))  / 15.0;
    let r = f32(extractBits(pixel, 0u, 4u))  / 15.0;

    textureStore(output_conversion_texture, id.xy, vec4f(r, g, b, a));
}
)"