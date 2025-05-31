R"(
@group(2) @binding(0) var texture: texture_2d<u32>;
@group(3) @binding(0) var clut : texture_1d<u32>;

fn fetchClut(index: u32) -> vec4u {
    let i = ((index >> renderData.clutShift) & renderData.clutMask) | (renderData.clutOffset & select(0x1FFu, 0xFFu, CLUT_FORMAT == 3));
    switch (CLUT_FORMAT) {
    case 3u: {
        return textureLoad(clut, i, 0);
    }
    default: {
        // Just some value that should be hard to miss
        return vec4u(255, 0, 0, 255);
    }
    }
}

fn fetchTexel(pos: vec2f, dims: vec2f) -> vec4u {
    var tex_pos = pos;
    if (U_CLAMP == 1) {
        tex_pos.x = clamp(tex_pos.x, 0.0, dims.x - 1);
    } else {
        tex_pos.x -= dims.x * floor(tex_pos.x / dims.x);
    }

    if (V_CLAMP == 1) {
        tex_pos.y = clamp(tex_pos.y, 0.0, dims.y - 1);
    } else {
        tex_pos.y -= dims.y * floor(tex_pos.y / dims.y);
    }

    var texel = textureLoad(texture, vec2u(tex_pos), 0);
    
    if (CLUT_FORMAT != 100) {
        texel = fetchClut(texel.r);
    }

    return texel;
}

fn filterTexture(uv: vec2f) -> vec4u {
    let dims = vec2f(textureDimensions(texture));
    let pos = uv * dims;

    return fetchTexel(pos, dims); 
}
)"