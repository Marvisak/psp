R"(
@group(1) @binding(1) var<uniform> vertex: Vertex;

@compute @workgroup_size(1, 1)
fn draw(@builtin(global_invocation_id) id: vec3u) {
    drawPixel(vec4i(vertex.pos), vertex.uv, vertex.color);
}
)"