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

fn drawPixel(pos: vec4i, uv: vec2f, c: vec4u) {
    if pos.x < renderData.scissorStart.x || pos.x > renderData.scissorEnd.x 
    ||  pos.y < renderData.scissorStart.y || pos.y > renderData.scissorEnd.y {
        return;
    }

    let dstDepth = textureLoad(depth_buffer, pos.xy).r;
    if DEPTH_FUNC != 100 {
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

    let dst = textureLoad(framebuffer, pos.xy);
    if BLEND_OPERATION != 100 {
        color = blend(vec4i(color), vec4i(dst));
    }
    color.a = dst.a;

    textureStore(framebuffer, pos.xy, color); 
    if DEPTH_WRITE == 1 {
        textureStore(depth_buffer, pos.xy, vec4u(pos.z));
    }
}
)"