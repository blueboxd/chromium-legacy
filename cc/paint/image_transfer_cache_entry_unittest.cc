// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

namespace cc {
namespace {

void MarkTextureAsReleased(SkImage::ReleaseContext context) {
  auto* released = static_cast<bool*>(context);
  DCHECK(!*released);
  *released = true;
}

// Uploads a texture corresponding to a single plane in a YUV image. All the
// samples in the plane are set to |value|. The texture is not owned by Skia:
// when Skia doesn't need it anymore, MarkTextureAsReleased() will be called.
sk_sp<SkImage> CreateSolidPlane(GrContext* gr_context,
                                int width,
                                int height,
                                uint8_t value,
                                bool* released) {
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixelsFlags(
          SkImageInfo::Make(width, height, kGray_8_SkColorType,
                            kOpaque_SkAlphaType),
          SkBitmap::kZeroPixels_AllocFlag)) {
    return nullptr;
  }
  bitmap.eraseColor(SkColorSetRGB(value, value, value));
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  if (!image)
    return nullptr;
  image = image->makeTextureImage(gr_context, nullptr /* dstColorSpace */,
                                  GrMipMapped::kNo);
  if (!image)
    return nullptr;

  // Take ownership of the backing texture;
  GrSurfaceOrigin origin;
  image->getBackendTexture(false /* flushPendingGrContextIO */, &origin);
  DCHECK_EQ(kGray_8_SkColorType, image->colorType());
  GrBackendTexture backend_texture;
  SkImage::BackendTextureReleaseProc release_proc;
  if (!SkImage::MakeBackendTextureFromSkImage(
          gr_context, std::move(image), &backend_texture, &release_proc)) {
    return nullptr;
  }

  *released = false;
  return SkImage::MakeFromTexture(gr_context, backend_texture, origin,
                                  kGray_8_SkColorType, kOpaque_SkAlphaType,
                                  nullptr /* colorSpace */,
                                  MarkTextureAsReleased, released);
}

// Checks if all the pixels in |image| are |expected_color|.
bool CheckImageIsSolidColor(const sk_sp<SkImage>& image,
                            SkColor expected_color) {
  DCHECK_GE(image->width(), 1);
  DCHECK_GE(image->height(), 1);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(image->width(), image->height()))
    return false;
  SkPixmap pixmap;
  if (!bitmap.peekPixels(&pixmap))
    return false;
  if (!image->readPixels(pixmap, 0 /* srcX */, 0 /* srcY */))
    return false;
  for (int y = 0; y < image->height(); y++) {
    for (int x = 0; x < image->width(); x++) {
      if (bitmap.getColor(x, y) != expected_color)
        return false;
    }
  }
  return true;
}

class ImageTransferCacheEntryTest : public testing::Test {
 public:
  void SetUp() override {
    // Initialize a GL GrContext for Skia.
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
    gl_context_ = base::MakeRefCounted<gl::GLContextEGL>(share_group_.get());
    ASSERT_TRUE(gl_context_);
    ASSERT_TRUE(
        gl_context_->Initialize(surface_.get(), gl::GLContextAttribs()));
    ASSERT_TRUE(gl_context_->MakeCurrent(surface_.get()));
    sk_sp<GrGLInterface> interface(gl::init::CreateGrGLInterface(
        *gl_context_->GetVersionInfo(), false /* use_version_es2 */));
    gr_context_ = GrContext::MakeGL(std::move(interface));
    ASSERT_TRUE(gr_context_);
  }

  // Creates the three textures for a 64x64 YUV 4:2:0 image where all the
  // samples in all planes are 255u. This corresponds to an RGB color of
  // (255, 121, 255) assuming the JPEG YUV-to-RGB conversion formulas. Returns a
  // list of 3 SkImages backed by the Y, U, and V textures in that order.
  // |release_flags| is set to a list of 3 boolean flags initialized to false.
  // Each flag corresponds to a plane (YUV order). When the texture for that
  // plane is released by Skia, that flag will be set to true. Returns an empty
  // vector on failure.
  std::vector<sk_sp<SkImage>> CreateTestYUVImage(
      std::array<bool, 3u>* release_flags) {
    *release_flags = {false, false, false};
    std::vector<sk_sp<SkImage>> plane_images = {
        CreateSolidPlane(gr_context(), 64, 64, 255u, &release_flags->at(0)),
        CreateSolidPlane(gr_context(), 32, 32, 255u, &release_flags->at(1)),
        CreateSolidPlane(gr_context(), 32, 32, 255u, &release_flags->at(2))};
    for (const auto& plane_image : plane_images) {
      if (plane_image)
        textures_to_free_.push_back(plane_image->getBackendTexture(false));
    }
    if (plane_images[0] && plane_images[1] && plane_images[2])
      return plane_images;
    return {};
  }

  void DeletePendingTextures() {
    DCHECK(gr_context_);
    for (const auto& texture : textures_to_free_) {
      if (texture.isValid())
        gr_context_->deleteBackendTexture(texture);
    }
    gr_context_->flush();
    textures_to_free_.clear();
  }

  void TearDown() override {
    DeletePendingTextures();
    gr_context_.reset();
    surface_->PrepareToDestroy(gl_context_->IsCurrent(surface_.get()));
    surface_.reset();
    gl_context_.reset();
    share_group_.reset();
  }

  GrContext* gr_context() const { return gr_context_.get(); }

 private:
  std::vector<GrBackendTexture> textures_to_free_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> gl_context_;
  sk_sp<GrContext> gr_context_;
};

TEST_F(ImageTransferCacheEntryTest, HardwareDecodedNoMipsAtCreation) {
  std::array<bool, 3u> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(3u, plane_images.size());

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains. The |buffer_byte_size| is only used for
  // accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images), 0u /* buffer_byte_size */,
      false /* needs_mips */, nullptr /* target_color_space */));

  // We didn't request generating mipmap chains, so the textures we created
  // above should stay alive until after the cache entry is deleted.
  EXPECT_FALSE(release_flags[0]);
  EXPECT_FALSE(release_flags[1]);
  EXPECT_FALSE(release_flags[2]);
  entry.reset();
  EXPECT_TRUE(release_flags[0]);
  EXPECT_TRUE(release_flags[1]);
  EXPECT_TRUE(release_flags[2]);
}

TEST_F(ImageTransferCacheEntryTest, HardwareDecodedMipsAtCreation) {
  std::array<bool, 3u> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(3u, plane_images.size());

  // Create a service-side image cache entry backed by these planes and request
  // generating mipmap chains at creation time. The |buffer_byte_size| is only
  // used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images), 0u /* buffer_byte_size */,
      true /* needs_mips */, nullptr /* target_color_space */));

  // We requested generating mipmap chains at creation time, so the textures we
  // created above should be released by now.
  EXPECT_TRUE(release_flags[0]);
  EXPECT_TRUE(release_flags[1]);
  EXPECT_TRUE(release_flags[2]);
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

TEST_F(ImageTransferCacheEntryTest, HardwareDecodedMipsAfterCreation) {
  std::array<bool, 3u> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(3u, plane_images.size());

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains at creation time. The |buffer_byte_size|
  // is only used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images), 0u /* buffer_byte_size */,
      false /* needs_mips */, nullptr /* target_color_space */));

  // We didn't request generating mip chains, so the textures we created above
  // should stay alive for now.
  EXPECT_FALSE(release_flags[0]);
  EXPECT_FALSE(release_flags[1]);
  EXPECT_FALSE(release_flags[2]);

  // Now request generating the mip chains.
  entry->EnsureMips();

  // Now the original textures should have been released.
  EXPECT_TRUE(release_flags[0]);
  EXPECT_TRUE(release_flags[1]);
  EXPECT_TRUE(release_flags[2]);
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

}  // namespace
}  // namespace cc
