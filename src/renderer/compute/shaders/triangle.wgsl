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
    var pos = vertices[3].pos + fid + 0.5;

    let e0 = edge(vertices[1].pos, vertices[2].pos, pos);
    let e1 = edge(vertices[2].pos, vertices[0].pos, pos);
    let e2 = edge(vertices[0].pos, vertices[1].pos, pos);
    if (e0 > 0 || (e0 == 0 && topLeft(vertices[1].pos, vertices[2].pos))) &&
       (e1 > 0 || (e1 == 0 && topLeft(vertices[2].pos, vertices[0].pos))) &&
       (e2 > 0 || (e2 == 0 && topLeft(vertices[0].pos, vertices[1].pos))) {
        let eSum = e0 + e1 + e2;
 
        let wq0 = e0 / vertices[0].pos.w;
        let wq1 = e1 / vertices[1].pos.w;
        let wq2 = e2 / vertices[2].pos.w;
        let uv = (vertices[0].uv * wq0 + vertices[1].uv * wq1 + vertices[2].uv * wq2) / (wq0 + wq1 + wq2);

        if vertices[0].pos.z != vertices[1].pos.z || vertices[0].pos.z != vertices[2].pos.z {
            pos.z = (vertices[0].pos.z * e0 + vertices[1].pos.z * e1 + vertices[2].pos.z * e2) / eSum;
        }

        var color = vertices[0].color;
        if GOURAUD_SHADING == 1 {
            let nColor0 = vec4f(vertices[0].color) / 255.0;
            let nColor1 = vec4f(vertices[1].color) / 255.0;
            let nColor2 = vec4f(vertices[2].color) / 255.0;

            color = vec4u((nColor0 * e0 + nColor1 * e1 + nColor2 * e2) / eSum * 255.0);
        }

        drawPixel(vec4i(pos), uv, color);
    }
}
)"