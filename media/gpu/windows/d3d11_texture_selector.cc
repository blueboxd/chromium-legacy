// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_selector.h"

#include <d3d11.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_surface_win.h"

namespace media {

TextureSelector::TextureSelector(VideoPixelFormat pixfmt,
                                 bool supports_swap_chain)
    : pixel_format_(pixfmt),
      supports_swap_chain_(supports_swap_chain) {
}

bool SupportsZeroCopy(const gpu::GpuPreferences& preferences,
                      const gpu::GpuDriverBugWorkarounds& workarounds) {
  if (!preferences.enable_zero_copy_dxgi_video)
    return false;

  if (workarounds.disable_dxgi_zero_copy_video)
    return false;

  return true;
}

// static
std::unique_ptr<TextureSelector> TextureSelector::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    DXGI_FORMAT decoder_output_format,
    MediaLog* media_log) {
  bool supports_nv12_decode_swap_chain =
      gl::DirectCompositionSurfaceWin::IsDecodeSwapChainSupported();
  bool needs_texture_copy = !SupportsZeroCopy(gpu_preferences, workarounds);

  VideoPixelFormat output_pixel_format;
  DXGI_FORMAT output_dxgi_format;

  // TODO(liberato): add other options here, like "copy to rgb" for NV12.
  // However, those require a pbuffer TextureWrapper implementation.
  switch (decoder_output_format) {
    case DXGI_FORMAT_NV12:
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing NV12";
      output_pixel_format = PIXEL_FORMAT_NV12;
      output_dxgi_format = DXGI_FORMAT_NV12;
      break;
    case DXGI_FORMAT_P010:
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing FP16";
      // Note: this combination isn't actually supported, since we don't support
      // pbuffer textures right now.
      output_pixel_format = PIXEL_FORMAT_ARGB;
      output_dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      // B8G8R8A8 is also an okay choice, if we don't have fp16 support.
      needs_texture_copy = true;
      break;
    default:
      // TODO(tmathmeyer) support other profiles in the future.
      MEDIA_LOG(INFO, media_log)
          << "D3D11VideoDecoder does not support " << decoder_output_format;
      return nullptr;
  }

  // Force texture copy on if requested for debugging.
  if (base::FeatureList::IsEnabled(kD3D11VideoDecoderAlwaysCopy))
    needs_texture_copy = true;

  if (needs_texture_copy) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is copying textures";
    return std::make_unique<CopyTextureSelector>(
        output_pixel_format, decoder_output_format, output_dxgi_format,
        supports_nv12_decode_swap_chain);  // TODO(tmathmeyer) false always?
  } else {
    // We don't support anything except NV12 for binding right now.  With
    // pbuffer textures, we could support rgb8 and / or fp16.
    DCHECK_EQ(output_pixel_format, PIXEL_FORMAT_NV12);
    return std::make_unique<TextureSelector>(output_pixel_format,
                                             supports_nv12_decode_swap_chain);
  }
}

std::unique_ptr<Texture2DWrapper> TextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    gfx::Size size) {
  // TODO(liberato): If the output format is rgb, then create a pbuffer wrapper.
  return std::make_unique<DefaultTexture2DWrapper>(size);
}

std::unique_ptr<Texture2DWrapper> CopyTextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    gfx::Size size) {
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = output_dxgifmt_;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

  // Decode swap chains do not support shared resources.
  // TODO(sunnyps): Find a workaround for when the decoder moves to its own
  // thread and D3D device.  See https://crbug.com/911847
  texture_desc.MiscFlags =
      supports_swap_chain_ ? 0 : D3D11_RESOURCE_MISC_SHARED;

  texture_desc.Width = size.width();
  texture_desc.Height = size.height();

  ComD3D11Texture2D out_texture;
  if (!SUCCEEDED(device->CreateTexture2D(&texture_desc, nullptr, &out_texture)))
    return nullptr;

  return std::make_unique<CopyingTexture2DWrapper>(
      size, std::make_unique<DefaultTexture2DWrapper>(size),
      std::make_unique<VideoProcessorProxy>(video_device, device_context),
      out_texture);
}

}  // namespace media
