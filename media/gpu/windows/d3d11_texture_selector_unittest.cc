// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "media/base/media_util.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;

namespace media {

class D3D11TextureSelectorUnittest : public ::testing::Test {
 public:
  VideoDecoderConfig CreateDecoderConfig(VideoCodecProfile profile,
                                         gfx::Size size,
                                         bool encrypted) {
    VideoDecoderConfig result;
    result.Initialize(
        kUnknownVideoCodec,  // It doesn't matter because it won't be used.
        profile, VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, size, {}, {}, {},
        encrypted ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted);
    return result;
  }

  std::unique_ptr<TextureSelector> CreateWithDefaultGPUInfo(
      DXGI_FORMAT decoder_output_format,
      bool zero_copy_enabled = true,
      TextureSelector::HDRMode hdr_mode = TextureSelector::HDRMode::kSDROnly) {
    gpu::GpuPreferences prefs;
    prefs.enable_zero_copy_dxgi_video = zero_copy_enabled;
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.disable_dxgi_zero_copy_video = false;
    auto media_log = std::make_unique<NullMediaLog>();
    return TextureSelector::Create(prefs, workarounds, decoder_output_format,
                                   hdr_mode, media_log.get());
  }
};

TEST_F(D3D11TextureSelectorUnittest, NV12BindsToNV12) {
  auto tex_sel = CreateWithDefaultGPUInfo(DXGI_FORMAT_NV12);

  // TODO(liberato): check "binds", somehow.
  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_NV12);
}

TEST_F(D3D11TextureSelectorUnittest, P010CopiesToFP16InHDR) {
  auto tex_sel = CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, true,
                                          TextureSelector::HDRMode::kSDROrHDR);

  // TODO(liberato): check "copies", somehow.
  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_ARGB);
  // Note that this might also produce 8 bit rgb, but for now always
  // tries for fp16.
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_R16G16B16A16_FLOAT);
  // TODO(liberato): Check output color space, somehow.
}

TEST_F(D3D11TextureSelectorUnittest, P010CopiesTo8BitInSDR) {
  auto tex_sel = CreateWithDefaultGPUInfo(DXGI_FORMAT_P010, true,
                                          TextureSelector::HDRMode::kSDROnly);

  // TODO(liberato): check "copies", somehow.
  EXPECT_EQ(tex_sel->PixelFormat(), PIXEL_FORMAT_ARGB);
  // Note that this might also produce 8 bit rgb, but for now always
  // tries for fp16.
  EXPECT_EQ(tex_sel->OutputDXGIFormat(), DXGI_FORMAT_B8G8R8A8_UNORM);
  // TODO(liberato): Check output color space, somehow.
}

}  // namespace media
