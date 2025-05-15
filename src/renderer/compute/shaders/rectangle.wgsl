R"(
struct Vertices {
    start: Vertex,
    end: Vertex 
}

@group(1) @binding(1) var<uniform> vertices: Vertices;

@compute @workgroup_size(8, 8)
fn draw(@builtin(global_invocation_id) id: vec3u) {
    let fid = vec4f(vec3f(id), 0.0);
    let pos = vertices.start.pos + fid;

    if (pos.x >= vertices.end.pos.x || pos.y >= vertices.end.pos.y) {
        return;
    }

    let t = (fid.xy + 0.5) / (vertices.end.pos.xy - vertices.start.pos.xy);
    let uv = vertices.start.uv + t * (vertices.end.uv - vertices.start.uv);

    drawPixel(vec4i(pos), uv, vertices.end.color);
}
)"