R"(
fn drawPixel(pos: vec4u, uv: vec2f, c: vec4u) {
    if (pos.x < renderData.scissorStart.x || pos.x > renderData.scissorEnd.x 
    ||  pos.y < renderData.scissorStart.y || pos.y > renderData.scissorEnd.y) {
        return;
    }

    var color = c;
    if (TEXTURES_ENABLED == 1) {
        color = filterTexture(uv);
    }

    textureStore(framebuffer, pos.xy, color); 
}
)"