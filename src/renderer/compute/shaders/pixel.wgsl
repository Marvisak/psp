R"(
fn blend(src: vec4i, dst: vec4i) -> vec4u {
    var result = vec4i(0);
    var asel = vec4i(0);
    var bsel = vec4i(0);

    if (BLEND_OPERATION <= 2) {
        switch (BLEND_SOURCE) {
        case 0u: { asel = dst; break; }
        case 1u: { asel = 255 - dst; break; }
        case 2u: { asel = vec4i(src.a); break; }
        case 3u: { asel = 255 - vec4i(src.a); break; }
        case 4u: { asel = vec4i(dst.a); break; }
        case 5u: { asel = 255 - vec4i(dst.a); break; }
        case 6u: { asel = vec4i(2 * src.a); break; }
        case 7u: { asel = vec4i(255 - min(2 * src.a, 255)); break; }
        case 8u: { asel = vec4i(2 * dst.a); break; }
        case 9u: { asel = vec4i(255 - min(2 * dst.a, 255)); break; }
        default: { asel = renderData.blendAFix; break; }
        }

        switch (BLEND_DESTINATION) {
        case 0u: { bsel = src; break; }
        case 1u: { bsel = 255 - src; break; }
        case 2u: { bsel = vec4i(src.a); break; }
        case 3u: { bsel = 255 - vec4i(src.a); break; }
        case 4u: { bsel = vec4i(dst.a); break; }
        case 5u: { bsel = 255 - vec4i(dst.a); break; }
        case 6u: { bsel = vec4i(2 * src.a); break; }
        case 7u: { bsel = vec4i(255 - min(2 * src.a, 255)); break; }
        case 8u: { bsel = vec4i(2 * dst.a); break; }
        case 9u: { bsel = vec4i(255 - min(2 * dst.a, 255)); break; }
        default: { bsel = renderData.blendBFix; break; }
        }
    }

    switch (BLEND_OPERATION) {
    case 0u: {
        result = ((src * 2 + 1) * (asel * 2 + 1) / 1024) + ((dst * 2 + 1) * (bsel * 2 + 1) / 1024);
    } 
    case 1u: {
        result = ((src * 2 + 1) * (asel * 2 + 1) / 1024) - ((dst * 2 + 1) * (bsel * 2 + 1) / 1024);
    } 
    case 2u: {
        result = ((dst * 2 + 1) * (bsel * 2 + 1) / 1024) - ((src * 2 + 1) * (asel * 2 + 1) / 1024);
    } 
    case 3u: { return vec4u(min(src, dst)); }
    case 4u: { return vec4u(max(src, dst)); }
    case 5u: { return vec4u(abs(src - dst)); }
    default: { break; }
    }

    return vec4u(clamp(result, vec4i(0), vec4i(255)));
}

fn drawPixel(pos: vec4u, uv: vec2f, c: vec4u) {
    if (pos.x < renderData.scissorStart.x || pos.x > renderData.scissorEnd.x 
    ||  pos.y < renderData.scissorStart.y || pos.y > renderData.scissorEnd.y) {
        return;
    }

    var color = c;
    if (TEXTURES_ENABLED == 1) {
        color = filterTexture(uv);
    }

    let dst = textureLoad(framebuffer, pos.xy);
    if (BLEND_OPERATION != 100) {
        color = blend(vec4i(color), vec4i(dst));
    }

    textureStore(framebuffer, pos.xy, color); 
}
)"