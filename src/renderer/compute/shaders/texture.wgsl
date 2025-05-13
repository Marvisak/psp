R"(
@group(2) @binding(0) var texture: texture_2d<u32>;
@group(2) @binding(1) var<storage, read> clut : array<u32, 256>;

fn u32ToRGBA8888(color: u32) -> vec4u {
    let a = (color >> 24) & 0xFF;
    let b = (color >> 16) & 0xFF;
    let g = (color >> 8)  & 0xFF;
    let r = (color >> 0)  & 0xFF;

    return vec4u(r, g, b, a);
}

fn RGB565ToRBA8888(color: u32) -> vec4u {
    let b = (color >> 11) & 0x1F; 
    let g = (color >> 5)  & 0x3F; 
    let r = (color >> 0)  & 0x1F; 

    return vec4u(   
        (r << 3) | (r >> 2),
        (g << 2) | (g >> 4),
        (b << 3) | (b >> 2),
        255
    );
}

fn fetch_clut(index: u32) -> vec4u {
    switch (CLUT_FORMAT) {
    case 0u: {
        let word = clut[index / 2];
        if (index % 2 == 0) {
            return RGB565ToRBA8888(word & 0xFFFF);
        } else {
            return RGB565ToRBA8888(word >> 16);
        }
    }
    case 3u: {
        return u32ToRGBA8888(clut[index]);
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
        texel = fetch_clut(texel.r);
    }

    return texel;
}

fn filterTexture(uv: vec2f) -> vec4u {
    let dims = vec2f(textureDimensions(texture));
    let pos = uv * dims;

    return fetchTexel(pos, dims); 
}
)"