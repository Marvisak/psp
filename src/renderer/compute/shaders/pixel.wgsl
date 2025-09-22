R"(
fn blend(src: vec4i, dst: vec4i) -> vec4u {
    var result = vec4i(0);
    var asel = vec4i(0);
    var bsel = vec4i(0);

    if BLEND_OPERATION <= 2 {
        switch BLEND_SOURCE {
            case 0u: { asel = dst; break; }
            case 1u: { asel = 255 - dst; break; }
            case 2u: { asel = vec4i(src.a); break; }
            case 3u: { asel = vec4i(255 - src.a); break; }
            case 4u: { asel = vec4i(dst.a); break; }
            case 5u: { asel = vec4i(255 - dst.a); break; }
            case 6u: { asel = vec4i(2 * src.a); break; }
            case 7u: { asel = vec4i(255 - min(2 * src.a, 255)); break; }
            case 8u: { asel = vec4i(2 * dst.a); break; }
            case 9u: { asel = vec4i(255 - min(2 * dst.a, 255)); break; }
            default: { asel = renderData.blendAFix; break; }
        }

        switch BLEND_DESTINATION {
            case 0u: { bsel = src; break; }
            case 1u: { bsel = 255 - src; break; }
            case 2u: { bsel = vec4i(src.a); break; }
            case 3u: { bsel = vec4i(255 - src.a); break; }
            case 4u: { bsel = vec4i(dst.a); break; }
            case 5u: { bsel = vec4i(255 - dst.a); break; }
            case 6u: { bsel = vec4i(2 * src.a); break; }
            case 7u: { bsel = vec4i(255 - min(2 * src.a, 255)); break; }
            case 8u: { bsel = vec4i(2 * dst.a); break; }
            case 9u: { bsel = vec4i(255 - min(2 * dst.a, 255)); break; }
            default: { bsel = renderData.blendBFix; break; }
        }
    }

    switch BLEND_OPERATION {
        case 0u: {
            result = ((src * 2 + 1) * (asel * 2 + 1) / 1024) + ((dst * 2 + 1) * (bsel * 2 + 1) / 1024);
            break;
        }
        case 1u: {
            result = ((src * 2 + 1) * (asel * 2 + 1) / 1024) - ((dst * 2 + 1) * (bsel * 2 + 1) / 1024);
            break;
        }
        case 2u: {
            result = ((dst * 2 + 1) * (bsel * 2 + 1) / 1024) - ((src * 2 + 1) * (asel * 2 + 1) / 1024);
            break;
        }
        case 3u: { return vec4u(min(src, dst)); }
        case 4u: { return vec4u(max(src, dst)); }
        case 5u: { return vec4u(abs(src - dst)); }
        default: { break; }
    }

    return vec4u(clamp(result, vec4i(0), vec4i(255)));
}

fn test(func: u32, src: u32, dst: u32) -> bool {
    switch func {
        case 0u: { return false; }
        case 1u: { return true; }
        case 2u: { return src == dst; }
        case 3u: { return src != dst; }
        case 4u: { return src < dst; }
        case 5u: { return src <= dst; }
        case 6u: { return src > dst; }
        case 7u: { return src >= dst; }
        default: { return true; }
    }
}

fn loadPixel(pos: vec2i) -> vec4u {
    switch (FRAMEBUFFER_FORMAT) {
    case 0u: {
        let pixel = textureLoad(framebuffer16, pos).r;
        let r = extractBits(pixel, 0u, 5u);
        let g = extractBits(pixel, 5u, 6u);
        let b = extractBits(pixel, 11u, 5u);

        return vec4u(
            (r << 3) | (r >> 2),
            (g << 2) | (g >> 4),
            (b << 3) | (b >> 2),
            0x00
        );
    }
    case 1u: {
        let pixel = textureLoad(framebuffer16, pos).r;
        let r = extractBits(pixel, 0u, 5u);
        let g = extractBits(pixel, 5u, 5u);
        let b = extractBits(pixel, 10u, 5u);
        let a = extractBits(pixel, 15u, 1u);

        return vec4u(
            (r << 3) | (r >> 2),
            (g << 3) | (g >> 2),
            (b << 3) | (b >> 2),
            select(0u, 255u, a == 1)
        );
    }
    case 2u: {
        let pixel = textureLoad(framebuffer16, pos).r;
        let r = extractBits(pixel, 0u, 4u);
        let g = extractBits(pixel, 4u, 4u);
        let b = extractBits(pixel, 8u, 4u);
        let a = extractBits(pixel, 12u, 4u);

        return vec4u(
            (r << 4) | r,
            (g << 4) | g,
            (b << 4) | b,
            (a << 4) | a
        );
    }
    case 3u: {
        return textureLoad(framebuffer, pos);
    }
    default: { break; }
    }

    return vec4u(0);
}

fn storePixel(pos: vec2i, color: vec4u) {
    switch (FRAMEBUFFER_FORMAT) {
    case 0u: {
        let r = color.r >> 3;
        let g = color.g >> 2;
        let b = color.b >> 3;

        let value = (b << 11) | (g << 5) | r;

        textureStore(framebuffer16, pos, vec4u(value));
    }
    case 1u: {
        let r = color.r >> 3;
        let g = color.g >> 3;
        let b = color.b >> 3;
        let a = select(0u, 1u, color.a > 0);

        let value = (a << 15) | (b << 10) | (g << 5) | r;
        textureStore(framebuffer16, pos, vec4u(value));
    }
    case 2u: {
        let r = color.r >> 4;
        let g = color.g >> 4;
        let b = color.b >> 4;
        let a = color.a >> 4;

        let value = (a << 12) | (b << 8) | (g << 4) | r;
        textureStore(framebuffer16, pos, vec4u(value));
    }
    case 3u: {
        textureStore(framebuffer, pos, color);
    }
    default: { break; }
    }
}

fn drawPixel(pos: vec4i, uv: vec2f, c: vec4u) {
    if pos.x < renderData.scissorStart.x || pos.x > renderData.scissorEnd.x 
    ||  pos.y < renderData.scissorStart.y || pos.y > renderData.scissorEnd.y {
        return;
    }

    if DEPTH_FUNC != 100 {
        let dstDepth = textureLoad(depthBuffer, pos.xy).r;
        if !test(DEPTH_FUNC, u32(pos.z), dstDepth) {
            return;
        }
    }

    var color = c;
    if TEXTURE_FORMAT != 100 {
        color = filterTexture(uv, color);
    }

    if ALPHA_FUNC != 100 {
        if !test(ALPHA_FUNC, color.a & renderData.alphaMask, renderData.alphaRef) {
            return;
        }
    }

    let dst = loadPixel(pos.xy);
    if BLEND_OPERATION != 100 {
        color = blend(vec4i(color), vec4i(dst));
    }
    color.a = dst.a;

    storePixel(pos.xy, color);
    if DEPTH_WRITE == 1 {
        textureStore(depthBuffer, pos.xy, vec4u(u32(pos.z)));
    }
}
)"