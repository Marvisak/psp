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
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var<uniform> frameWidth: f32;
@group(0) @binding(3) var<uniform> transform: ScreenTransform;

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
    uv.x = uv.x % (frameWidth / 480.0);
    return textureSample(texture, textureSampler, uv).rgba;
}
)"