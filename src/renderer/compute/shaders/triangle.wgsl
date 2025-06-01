R"(
@group(1) @binding(1) var<uniform> vertices: array<Vertex, 4>;

fn edge(v0: vec4f, v1: vec4f, v2: vec4f) -> f32 {
    return (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
}

fn topLeft(v0: vec4f, v1: vec4f) -> bool {
    return (v0.y == v1.y && v1.x > v0.x) || (v0.y > v1.y);
}

@compute @workgroup_size(8, 8)
fn draw(@builtin(global_invocation_id) id: vec3u) {
    let fid = vec4f(vec3f(id), 0.0);
    let pos = vertices[3].pos + fid + 0.5;

    let bias0 = select(-1.0, 0.0, topLeft(vertices[1].pos, vertices[2].pos));
    let bias1 = select(-1.0, 0.0, topLeft(vertices[2].pos, vertices[0].pos));
    let bias2 = select(-1.0, 0.0, topLeft(vertices[0].pos, vertices[1].pos));

    let e0 = edge(vertices[1].pos, vertices[2].pos, pos);
    let e1 = edge(vertices[2].pos, vertices[0].pos, pos);
    let e2 = edge(vertices[0].pos, vertices[1].pos, pos);
    if (e0 + bias0) >= 0 && (e1 + bias1) >= 0 && (e2 + bias2) >= 0 {
        let eSum = e0 + e1 + e2;
        let uv = (vertices[0].uv * e0 + vertices[1].uv * e1 + vertices[2].uv * e2) / eSum;

        drawPixel(vec4i(pos), uv, vertices[0].color);
    }
}
)"