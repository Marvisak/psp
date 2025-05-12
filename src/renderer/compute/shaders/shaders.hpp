#pragma once

constexpr const char framebuffer_shader[] =
#include "framebuffer.wgsl"
;

constexpr const char common_shader[] =
#include "common.wgsl"
;

constexpr const char texture_shader[] =
#include "texture.wgsl"
;

constexpr const char pixel_shader[] =
#include "pixel.wgsl"
;

constexpr const char rectangle_shader[] =
#include "rectangle.wgsl"
;