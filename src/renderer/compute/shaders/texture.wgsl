R"(
@group(2) @binding(0) var texture: texture_2d<u32>;
@group(3) @binding(0) var clut : texture_1d<u32>;

fn fetchClut(index: u32) -> vec4u {
    let i = ((index >> renderData.clutShift) & renderData.clutMask) | (renderData.clutOffset & select(0x1FFu, 0xFFu, CLUT_FORMAT == 3));
    switch CLUT_FORMAT {
        case 3u: {
            return textureLoad(clut, i, 0);
        }
        default: { return vec4u(255, 0, 0, 255); }
    }
}

fn fetchTexel(pos: vec2f, dims: vec2f) -> vec4u {
    var tex_pos = pos;
    if U_CLAMP == 1 {
        tex_pos.x = clamp(tex_pos.x, 0.0, dims.x - 1);
    } else {
        tex_pos.x -= dims.x * floor(tex_pos.x / dims.x);
    }

    if V_CLAMP == 1 {
        tex_pos.y = clamp(tex_pos.y, 0.0, dims.y - 1);
    } else {
        tex_pos.y -= dims.y * floor(tex_pos.y / dims.y);
    }

    var u_tex_pos = vec2u(tex_pos);
    switch TEXTURE_FORMAT {
        case 2u: {
            u_tex_pos.x *= 2;
            let lower = textureLoad(texture, u_tex_pos, 0).r;
            let higher = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;

            let a = extractBits(lower, 0u, 4u);
            let b = extractBits(lower, 4u, 4u);
            let g = extractBits(higher, 0u, 4u);
            let r = extractBits(higher, 4u, 4u);

            return vec4u((r << 4) | r, (g << 4) | g, (b << 4) | b, (a << 4) | a);
        }
        case 3u: {
            u_tex_pos.x *= 4;
            let r = textureLoad(texture, u_tex_pos, 0).r;
            let g = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;
            let b = textureLoad(texture, vec2u(u_tex_pos.x + 2, u_tex_pos.y), 0).r;
            let a = textureLoad(texture, vec2u(u_tex_pos.x + 3, u_tex_pos.y), 0).r;

            return vec4u(r, g, b, a);
        }
        case 4u: {
            var index = textureLoad(texture, vec2u(u_tex_pos.x / 2, u_tex_pos.y), 0).r;
            index = extractBits(index, select(4u, 0u, u_tex_pos.x % 2 == 0), 4u);
            return fetchClut(index);
        }
        case 5u: {
            let index = textureLoad(texture, u_tex_pos, 0).r;
            return fetchClut(index);
        }
        case 7u: {
            let b1 = textureLoad(texture, u_tex_pos, 0).r;
            let b2 = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;
            let b3 = textureLoad(texture, vec2u(u_tex_pos.x + 2, u_tex_pos.y), 0).r;
            let b4 = textureLoad(texture, vec2u(u_tex_pos.x + 3, u_tex_pos.y), 0).r;
            return fetchClut((b4 << 24) | (b3 << 16) | (b2 << 8) | b1);
        }
        default: { return vec4u(255, 0, 0, 255); }
    }
}

fn blendTexture(texel: vec4i, color: vec4i) -> vec4u {
    var result = vec4i(0);
    switch TEXTURE_FUNCTION {
        case 0u: {
            if FRAGMENT_DOUBLE == 1 {
                result  = ((color + 1) * texel * 2) / 256;
            } else {
                result  = (color + 1) * texel / 256;
            }
            result.a = select(color.a, (color.a + 1) * texel.a / 256, USE_TEXTURE_ALPHA == 1);
            break;
        }
        case 1u: {
            if USE_TEXTURE_ALPHA == 1 {
                result = ((color + 1) * (255 - texel.a) + (texel + 1) * texel.a);
                result /= select(256, 128, FRAGMENT_DOUBLE == 1);
            } else {
                result = select(texel, texel * 2, FRAGMENT_DOUBLE == 1);
            }
            result.a = color.a;
            break;
        }
        case 2u: {
            result = ((255 - texel) * color + texel * renderData.environmentTexture + 255);
            result /= select(256, 128, FRAGMENT_DOUBLE == 1);
            result.a = select(color.a, (color.a + 1) * texel.a / 256, USE_TEXTURE_ALPHA == 1);
            break;
        }
        case 3u: {
            result = select(texel, texel * 2, FRAGMENT_DOUBLE == 1);
            result.a = select(color.a, texel.a, USE_TEXTURE_ALPHA == 1);
            break;
        }
        case 4u: {
            result = texel + color;
            if FRAGMENT_DOUBLE == 1 {
                result *= 2;
            }
            result.a = select(color.a, (color.a + 1) * texel.a / 256, USE_TEXTURE_ALPHA == 1);
            break;
        }
        default: { break; }
    }
    return vec4u(clamp(result, vec4i(0), vec4i(255)));
}

fn filterTexture(uv: vec2f, color: vec4u) -> vec4u {
    var dims = vec2f(textureDimensions(texture));
    switch TEXTURE_FORMAT {
        case 2u: { dims.x /= 2; break; }
        case 3u: { dims.x /= 4; break; }
        case 4u: { dims.x *= 2; break; }
        case 7u: { dims.x /= 4; break; }
        default: { break; }
    }

    let pos = uv * dims;
    var texel = vec4u(0);

    if BILINEAR == 1 {
        let f = fract(pos - 0.5);
        let top_left = floor(pos - 0.5);

        let tl = vec4f(fetchTexel(vec2f(top_left.x + 0.5, top_left.y + 0.5), dims)) / 255;
        let tr = vec4f(fetchTexel(vec2f(top_left.x + 1.5, top_left.y + 0.5), dims)) / 255;
        let bl = vec4f(fetchTexel(vec2f(top_left.x + 0.5, top_left.y + 1.5), dims)) / 255;
        let br = vec4f(fetchTexel(vec2f(top_left.x + 1.5, top_left.y + 1.5), dims)) / 255;

        texel = vec4u(mix(mix(tl, tr, f.x), mix(bl, br, f.x), f.y) * 255);
    } else {
        texel = fetchTexel(pos, dims);
    }

    return blendTexture(vec4i(texel), vec4i(color));
}
)"