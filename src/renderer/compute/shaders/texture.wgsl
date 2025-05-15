R"(
@group(2) @binding(0) var texture: texture_2d<u32>;
@group(2) @binding(1) var<storage, read> clut : array<u32, 256>;

fn RGB565ToRGBA8888(color: u32) -> vec4u {
    let b = extractBits(color, 11u, 5u);
    let g = extractBits(color, 5u, 6u);
    let r = extractBits(color, 0u, 5u);

    return vec4u(   
        (r << 3) | (r >> 2),
        (g << 2) | (g >> 4),
        (b << 3) | (b >> 2),
        255
    );
}

fn RGBA4444ToRGBA8888(color: u32) -> vec4u {
    let a = extractBits(color, 12u, 4u);
    let b = extractBits(color, 8u, 4u);
    let g = extractBits(color, 4u, 4u);
    let r = extractBits(color, 0u, 4u);

    return vec4u(
        (r << 4u) | r,
        (g << 4u) | g,
        (b << 4u) | b,
        (a << 4u) | a,
    );
}

fn fetchClut(index: u32) -> vec4u {
    let i = ((index >> renderData.clutShift) & renderData.clutMask) | (renderData.clutOffset & select(0x1FFu, 0xFFu, CLUT_FORMAT == 3));

    switch (CLUT_FORMAT) {
    case 0u: {
        let word = clut[i / 2];
        if (i % 2 == 0) {
            return RGB565ToRGBA8888(word & 0xFFFF);
        } else {
            return RGB565ToRGBA8888(word >> 16);
        }
    }
    case 2u: {
        let word = clut[i / 2];
        if (i % 2 == 0) {
            return RGBA4444ToRGBA8888(word & 0xFFFF);
        } else {
            return RGBA4444ToRGBA8888(word >> 16);
        }
    }
    case 3u: {
        return unpack4xU8(clut[i]);
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
        texel = fetchClut(pack4xU8(texel));
    }

    return texel;
}

fn filterTexture(uv: vec2f) -> vec4u {
    let dims = vec2f(textureDimensions(texture));
    let pos = uv * dims;

    return fetchTexel(pos, dims); 
}
)"