// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace {
bool IsPowerOfTwo(uint32_t value) {
  return value && (value & (value - 1)) == 0;
}
}  // namespace

namespace blink {

XRCubeMap::XRCubeMap(const device::mojom::blink::XRCubeMap& cube_map) {
  constexpr auto kNumComponentsPerPixel =
      device::mojom::blink::XRCubeMap::kNumComponentsPerPixel;
  static_assert(kNumComponentsPerPixel == 4,
                "XRCubeMaps are expected to be in the RGBA16F format");

  // Cube map sides must all be a power-of-two image
  bool valid = IsPowerOfTwo(cube_map.width_and_height);
  const size_t expected_size =
      cube_map.width_and_height * cube_map.width_and_height;
  valid &= cube_map.positive_x.size() == expected_size;
  valid &= cube_map.negative_x.size() == expected_size;
  valid &= cube_map.positive_y.size() == expected_size;
  valid &= cube_map.negative_y.size() == expected_size;
  valid &= cube_map.positive_z.size() == expected_size;
  valid &= cube_map.negative_z.size() == expected_size;
  DCHECK(valid);

  width_and_height_ = cube_map.width_and_height;
  positive_x_ = cube_map.positive_x;
  negative_x_ = cube_map.negative_x;
  positive_y_ = cube_map.positive_y;
  negative_y_ = cube_map.negative_y;
  positive_z_ = cube_map.positive_z;
  negative_z_ = cube_map.negative_z;
}

WebGLTexture* XRCubeMap::updateWebGLEnvironmentCube(
    WebGLRenderingContextBase* context,
    WebGLTexture* texture) const {
  // Ensure a texture was supplied from the passed context and with an
  // appropriate bound target.
  DCHECK(texture);
  DCHECK(!texture->HasEverBeenBound() ||
         texture->GetTarget() == GL_TEXTURE_CUBE_MAP);
  DCHECK(texture->ContextGroup() == context->ContextGroup());

  auto* gl = context->ContextGL();
  texture->SetTarget(GL_TEXTURE_CUBE_MAP);
  gl->BindTexture(GL_TEXTURE_CUBE_MAP, texture->Object());

  // Cannot generate mip-maps for half-float textures, so use linear filtering
  gl->TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  uint16_t const* const cubemap_images[] = {
      positive_x_.data()->components, negative_x_.data()->components,
      positive_y_.data()->components, negative_y_.data()->components,
      positive_z_.data()->components, negative_z_.data()->components,
  };
  GLenum const cubemap_targets[] = {
      GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };

  // Update image for each side of the cube map
  for (int i = 0; i < 6; ++i) {
    auto* image = cubemap_images[i];
    auto target = cubemap_targets[i];

    gl->TexImage2D(target, 0, GL_RGBA16F, width_and_height_, width_and_height_,
                   0, GL_RGBA, GL_HALF_FLOAT, image);
  }

  // TODO(https://crbug.com/1079007): Restore the texture binding
  // gl->BindTexture(GL_TEXTURE_CUBE_MAP, 0);
  DrawingBuffer::Client* client = static_cast<DrawingBuffer::Client*>(context);
  client->DrawingBufferClientRestoreTextureCubeMapBinding();

  // Debug check for success
  DCHECK(gl->GetError() == GL_NO_ERROR);

  return texture;
}

}  // namespace blink
