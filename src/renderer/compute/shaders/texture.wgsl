R"(
@group(2) @binding(0) var texture: texture_2d<u32>;
@group(3) @binding(0) var clut : texture_1d<u32>;

fn fetchDXTColor(block_pos: vec2u, local_pos: vec2u, alpha: u32) -> vec4u {
    let color1 = (textureLoad(texture, vec2u(block_pos.x + 5, block_pos.y), 0).r << 8) | textureLoad(texture, vec2u(block_pos.x + 4, block_pos.y), 0).r;
    let color2 = (textureLoad(texture, vec2u(block_pos.x + 7, block_pos.y), 0).r << 8) | textureLoad(texture, vec2u(block_pos.x + 6, block_pos.y), 0).r;

    let rgb1 = vec4u((color1 >> 8) & 0xF8, (color1 >> 3) & 0xFC, (color1 << 3) & 0xF8, alpha);
    let rgb2 = vec4u((color2 >> 8) & 0xF8, (color2 >> 3) & 0xFC, (color2 << 3) & 0xF8, alpha);

    let color_index = (textureLoad(texture, vec2u(block_pos.x + local_pos.y, block_pos.y), 0).r >> (local_pos.x * 2)) & 3;

    if color_index == 0 {
        return rgb1;
    } else if color_index == 1 {
        return rgb2;
    } else if color1 > color2 {
        if color_index == 2 {
            return (rgb1 + rgb1 + rgb2) / 3;
        }
        return (rgb1 + rgb2 + rgb2) / 3;
    } else if (color_index == 3) {
        return vec4u(0);
    }

    return (rgb1 + rgb2) / 2;
}

fn fetchClut(index: u32) -> vec4u {
    let i = ((index >> renderData.clutShift) & renderData.clutMask) | (renderData.clutOffset & select(0x1FFu, 0xFFu, CLUT_FORMAT == 3));
    
    if (CLUT_FORMAT == 3u) {
        return textureLoad(clut, i, 0);
    } else {
        let real_index = i / 2u;
        let value = textureLoad(clut, real_index, 0);
        let color = select((value.r << 8) | value.g, (value.a << 8) | value.b, i % 2 == 0);
    
        switch CLUT_FORMAT {
            case 2u: {
                let a = extractBits(color, 12u, 4u);
                let b = extractBits(color, 8u, 4u);
                let g = extractBits(color, 4u, 4u);
                let r = extractBits(color, 0u, 4u);

                return vec4u((r << 4) | r, (g << 4) | g, (b << 4) | b, (a << 4) | a);
            }
            default: { return vec4u(255, 0, 0, 255); }
        }
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
        case 0u: {
            u_tex_pos.x *= 2;
            let lower = textureLoad(texture, u_tex_pos, 0).r;
            let higher = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;

            let value = (higher << 8u) | lower;

            let r = extractBits(value, 0u, 5u);
            let g = extractBits(value, 5u, 6u);
            let b = extractBits(value, 11u, 5u);

            return vec4u(
                (r << 3) | (r >> 2),
                (g << 2) | (g >> 4),
                (b << 3) | (b >> 2),
                0xFF
            );
        }
        case 1u: {
            u_tex_pos.x *= 2;
            let lower = textureLoad(texture, u_tex_pos, 0).r;
            let higher = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;

            let value = (higher << 8u) | lower;

            let a = extractBits(value, 15u, 1u);
            let b = extractBits(value, 10u, 5u);
            let g = extractBits(value, 5u, 5u);
            let r = extractBits(value, 0u, 5u);

            return vec4u((r << 3u) | (r >> 2u), (g << 3u) | (g >> 2u), (b << 3u) | (b >> 2u), a * 255);
        }
        case 2u: {
            u_tex_pos.x *= 2;
            let lower = textureLoad(texture, u_tex_pos, 0).r;
            let higher = textureLoad(texture, vec2u(u_tex_pos.x + 1, u_tex_pos.y), 0).r;

            let a = extractBits(higher, 4u, 4u);
            let b = extractBits(higher, 0u, 4u);
            let g = extractBits(lower, 4u, 4u);
            let r = extractBits(lower, 0u, 4u);

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
        case 8u: {
            let local_pos = u_tex_pos % 4;
            let block_pos = vec2u((u_tex_pos.x - local_pos.x) * 2, u_tex_pos.y / 4);
            return fetchDXTColor(block_pos, local_pos, 0xFF);
        }
        case 9u: {
            let local_pos = u_tex_pos % 4;
            let block_pos = vec2u((u_tex_pos.x - local_pos.x) * 4, u_tex_pos.y / 4);
            let color = fetchDXTColor(block_pos, local_pos, 0);

            let alpha = ((textureLoad(texture, vec2u(block_pos.x + 9 + local_pos.y * 2, block_pos.y), 0).r << 8)
                       | textureLoad(texture, vec2u(block_pos.x + 8 + local_pos.y * 2, block_pos.y), 0).r);
            return vec4u(color.r, color.g, color.b, ((alpha >> (local_pos.x * 4)) & 0xF) << 4);
        }
        case 10u: {
            let local_pos = u_tex_pos % 4;
            let block_pos = vec2u((u_tex_pos.x - local_pos.x) * 4, u_tex_pos.y / 4);
            let color = fetchDXTColor(block_pos, local_pos, 0);

            let alpha_data2 = ((textureLoad(texture, vec2u(block_pos.x + 11, block_pos.y), 0).r << 24)
                             | (textureLoad(texture, vec2u(block_pos.x + 10, block_pos.y), 0).r << 16)
                             | (textureLoad(texture, vec2u(block_pos.x + 9, block_pos.y), 0).r << 8)
                             | (textureLoad(texture, vec2u(block_pos.x + 8, block_pos.y), 0).r));

            let alpha_data1 = ((textureLoad(texture, vec2u(block_pos.x + 13, block_pos.y), 0).r << 8)
                             | (textureLoad(texture, vec2u(block_pos.x + 12, block_pos.y), 0).r));
            

            // WGSL doesn't support 64bit numbers, yikes
            var alpha_index = 0u;
            let alpha_index_bit = local_pos.y * 12 + local_pos.x * 3;
            if ((alpha_index_bit + 3) <= 32) {
                alpha_index = extractBits(alpha_data2, alpha_index_bit, 3u);
            } else if (alpha_index_bit >= 32) {
                alpha_index = extractBits(alpha_data1, alpha_index_bit - 32, 3u);
            } else {
                let lo_bits = 32u - alpha_index_bit;
                let hi_bits = 3u - lo_bits;

                let lo = extractBits(alpha_data2, alpha_index_bit, ((1u << lo_bits) - 1u));
                let hi = extractBits(alpha_data1, 0u, ((1u << hi_bits) - 1u));
                alpha_index = (hi << lo_bits) | lo;
            }

            let alpha1 = textureLoad(texture, vec2u(block_pos.x + 14, block_pos.y), 0).r;
            let alpha2 = textureLoad(texture, vec2u(block_pos.x + 15, block_pos.y), 0).r;
            if alpha_index == 0 {
                return vec4u(color.r, color.g, color.b, alpha1);
            } else if alpha_index == 1 {
                return vec4u(color.r, color.g, color.b, alpha2);
            } else if alpha1 > alpha2 {
                let lerp_alpha1 = (alpha1 * ((7 - (alpha_index - 1)) << 8)) / 7;
                let lerp_alpha2 = (alpha2 * ((alpha_index - 1) << 8)) / 7;
                return vec4u(color.r, color.g, color.b, ((lerp_alpha1 + lerp_alpha2 + 31) >> 8) & 0xFF);
            } else if alpha_index == 6 {
                return color;
            } else if alpha_index == 7 {
                return vec4u(color.r, color.g, color.b, 0xFF);
            }

            let lerp_alpha1 = (alpha1 * ((5 - (alpha_index - 1)) << 8)) / 5;
            let lerp_alpha2 = (alpha2 * ((alpha_index - 1) << 8)) / 5;
            return vec4u(color.r, color.g, color.b, ((lerp_alpha1 + lerp_alpha2 + 31) >> 8) & 0xFF);
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
        case 8u: { dims.x /= 2; dims.y *= 4; break; }
        case 9u: { dims.x /= 4; dims.y *= 4; break; }
        case 10u: { dims.x /= 4; dims.y *= 4; break; }
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