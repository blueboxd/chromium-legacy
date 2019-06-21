// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_selector.h"

#include <d3d11.h>

#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TextureSelector::TextureSelector(VideoPixelFormat pixfmt,
                                 DXGI_FORMAT dxgifmt,
                                 GUID decoder_guid,
                                 gfx::Size coded_size,
                                 bool is_encrypted,
                                 bool supports_swap_chain)
    : pixel_format_(pixfmt),
      dxgi_format_(dxgifmt),
      decoder_guid_(decoder_guid),
      coded_size_(coded_size),
      is_encrypted_(is_encrypted),
      supports_swap_chain_(supports_swap_chain) {
  SetUpDecoderDescriptor();
  SetUpTextureDescriptor();
}

// static
std::unique_ptr<TextureSelector> TextureSelector::Create(
    const VideoDecoderConfig& config) {
  // TODO(tmathmeyer) use a CopyTextureSelector
  if (config.profile() == VP9PROFILE_PROFILE2) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_YUV420P10, DXGI_FORMAT_P010,
        D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2, config.coded_size(),
        config.is_encrypted(), false);
  }

  bool supports_nv12_decode_swap_chain = base::FeatureList::IsEnabled(
      features::kDirectCompositionUseNV12DecodeSwapChain);

  if (config.profile() == VP9PROFILE_PROFILE0) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_NV12, DXGI_FORMAT_NV12,
        D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, config.coded_size(),
        config.is_encrypted(), supports_nv12_decode_swap_chain);
  }

  if (config.codec() == kCodecH264) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_NV12, DXGI_FORMAT_NV12,
        D3D11_DECODER_PROFILE_H264_VLD_NOFGT, config.coded_size(),
        config.is_encrypted(), supports_nv12_decode_swap_chain);
  }

  return nullptr;
}

bool TextureSelector::SupportsDevice(
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device) {
  for (UINT i = video_device->GetVideoDecoderProfileCount(); i--;) {
    GUID profile = {};
    if (SUCCEEDED(video_device->GetVideoDecoderProfile(i, &profile))) {
      if (profile == decoder_guid_)
        return true;
    }
  }
  return false;
}

ComD3D11Texture2D TextureSelector::CreateOutputTexture(ComD3D11Device device,
                                                       gfx::Size size) {
  texture_desc_.Width = size.width();
  texture_desc_.Height = size.height();

  ComD3D11Texture2D result;
  if (!SUCCEEDED(device->CreateTexture2D(&texture_desc_, nullptr, &result)))
    return nullptr;

  return result;
}

std::unique_ptr<Texture2DWrapper> TextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    ComD3D11Texture2D input_texture,
    gfx::Size size) {
  return std::make_unique<DefaultTexture2DWrapper>(input_texture);
}

// private
void TextureSelector::SetUpDecoderDescriptor() {
  decoder_desc_ = {};
  decoder_desc_.Guid = decoder_guid_;
  decoder_desc_.SampleWidth = coded_size_.width();
  decoder_desc_.SampleHeight = coded_size_.height();
  decoder_desc_.OutputFormat = dxgi_format_;
}

// private
void TextureSelector::SetUpTextureDescriptor() {
  texture_desc_ = {};
  texture_desc_.MipLevels = 1;
  texture_desc_.ArraySize = TextureSelector::BUFFER_COUNT;
  texture_desc_.Format = dxgi_format_;
  texture_desc_.SampleDesc.Count = 1;
  texture_desc_.Usage = D3D11_USAGE_DEFAULT;
  texture_desc_.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

  // Decode swap chains do not support shared resources.
  // TODO(sunnyps): Find a workaround for when the decoder moves to its own
  // thread and D3D device.  See https://crbug.com/911847
  texture_desc_.MiscFlags =
      supports_swap_chain_ ? 0 : D3D11_RESOURCE_MISC_SHARED;

  if (is_encrypted_)
    texture_desc_.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
}

std::unique_ptr<Texture2DWrapper> CopyTextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    ComD3D11Texture2D input_texture,
    gfx::Size size) {
  // Change the texture descriptor flags to make different output textures
  texture_desc_.BindFlags =
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  texture_desc_.ArraySize = 1;
  texture_desc_.CPUAccessFlags = 0;

  ComD3D11Texture2D out_texture = CreateOutputTexture(device, size);
  if (!out_texture)
    return nullptr;

  return std::make_unique<CopyingTexture2DWrapper>(
      std::make_unique<DefaultTexture2DWrapper>(out_texture),
      std::make_unique<VideoProcessorProxy>(video_device, device_context),
      input_texture);
}

}  // namespace media
