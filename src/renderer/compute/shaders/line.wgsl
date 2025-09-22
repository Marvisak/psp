R"(
struct Vertices {
    start: Vertex,
    end: Vertex 
}

@group(1) @binding(1) var<uniform> vertices: Vertices;

@compute @workgroup_size(8, 8)
fn draw(@builtin(global_invocation_id) id: vec3u) {
    let fid = vec4f(vec3f(id), 0.0);
    let length = ceil(length(vertices.end.pos - vertices.start.pos));
    let t = fid.x / length;

    let pos = mix(vertices.start.pos, vertices.end.pos, t);

    if pos.x > vertices.end.pos.x || pos.y > vertices.end.pos.y {
        return;
    }

    let uv = mix(vertices.start.uv, vertices.end.uv, t); 

    drawPixel(vec4i(pos), uv, vertices.end.color);
}
)"