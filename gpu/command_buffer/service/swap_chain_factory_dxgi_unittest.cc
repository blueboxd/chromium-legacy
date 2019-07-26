// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/swap_chain_factory_dxgi.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi_swap_chain.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SwapChainFactoryDXGITest : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

 protected:
  bool UsesPassthrough() const {
    return gles2::PassthroughCommandDecoderSupported();
  }
  void CreateAndPresentSwapChain(bool uses_passthrough_texture);

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
};

void SwapChainFactoryDXGITest::CreateAndPresentSwapChain(
    bool uses_passthrough_texture) {
  DCHECK(SwapChainFactoryDXGI::IsSupported());
  std::unique_ptr<SwapChainFactoryDXGI> swap_chain_factory_ =
      std::make_unique<SwapChainFactoryDXGI>(uses_passthrough_texture);
  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                   gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto backings = swap_chain_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      usage);
  ASSERT_TRUE(backings.front_buffer);
  ASSERT_TRUE(backings.back_buffer);

  GLenum expected_target = GL_TEXTURE_2D;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  scoped_refptr<gles2::TexturePassthrough> back_passthrough_texture;
  gles2::Texture* back_texture = nullptr;
  gl::GLImageDXGISwapChain* back_image = nullptr;
  gl::GLImageDXGISwapChain* front_image = nullptr;

  std::unique_ptr<SharedImageRepresentationFactoryRef> back_factory_ref =
      shared_image_manager_.Register(std::move(backings.back_buffer),
                                     memory_type_tracker_.get());
  std::unique_ptr<SharedImageRepresentationFactoryRef> front_factory_ref =
      shared_image_manager_.Register(std::move(backings.front_buffer),
                                     memory_type_tracker_.get());

  if (uses_passthrough_texture) {
    auto back_gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            back_buffer_mailbox);
    ASSERT_TRUE(back_gl_representation);
    back_passthrough_texture = back_gl_representation->GetTexturePassthrough();
    EXPECT_TRUE(back_passthrough_texture);
    EXPECT_EQ(TextureBase::Type::kPassthrough,
              back_passthrough_texture->GetType());
    EXPECT_EQ(expected_target, back_passthrough_texture->target());
    back_gl_representation.reset();

    back_image = gl::GLImageDXGISwapChain::FromGLImage(
        back_passthrough_texture->GetLevelImage(
            back_passthrough_texture->target(), 0));
    ASSERT_TRUE(back_image);

    auto front_gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            front_buffer_mailbox);
    ASSERT_TRUE(front_gl_representation);
    auto front_passthrough_texture =
        front_gl_representation->GetTexturePassthrough();
    EXPECT_TRUE(front_passthrough_texture);
    EXPECT_EQ(TextureBase::Type::kPassthrough,
              front_passthrough_texture->GetType());
    EXPECT_EQ(expected_target, front_passthrough_texture->target());
    front_gl_representation.reset();

    front_image = gl::GLImageDXGISwapChain::FromGLImage(
        front_passthrough_texture->GetLevelImage(
            front_passthrough_texture->target(), 0));
    ASSERT_TRUE(front_image);
    front_passthrough_texture.reset();
  } else {
    auto back_gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(
            back_buffer_mailbox);
    ASSERT_TRUE(back_gl_representation);
    back_texture = back_gl_representation->GetTexture();
    EXPECT_TRUE(back_texture);
    EXPECT_EQ(TextureBase::Type::kValidated, back_texture->GetType());
    EXPECT_EQ(expected_target, back_texture->target());
    // Ensures that back buffer is explicitly cleared.
    EXPECT_TRUE(back_texture->IsLevelCleared(back_texture->target(), 0));
    back_gl_representation.reset();

    gles2::Texture::ImageState image_state;
    back_image = gl::GLImageDXGISwapChain::FromGLImage(
        back_texture->GetLevelImage(back_texture->target(), 0, &image_state));
    ASSERT_TRUE(back_image);
    EXPECT_EQ(gles2::Texture::BOUND, image_state);

    auto front_gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(
            front_buffer_mailbox);
    ASSERT_TRUE(front_gl_representation);
    gles2::Texture* front_texture = front_gl_representation->GetTexture();
    EXPECT_TRUE(front_texture);
    EXPECT_EQ(TextureBase::Type::kValidated, front_texture->GetType());
    EXPECT_EQ(expected_target, front_texture->target());
    // Ensures that front buffer is explicitly cleared.
    EXPECT_TRUE(front_texture->IsLevelCleared(front_texture->target(), 0));
    front_gl_representation.reset();

    front_image = gl::GLImageDXGISwapChain::FromGLImage(
        front_texture->GetLevelImage(front_texture->target(), 0, &image_state));
    ASSERT_TRUE(front_image);
    EXPECT_EQ(gles2::Texture::BOUND, image_state);
  }

  EXPECT_EQ(S_OK, back_image->swap_chain()->GetBuffer(
                      0 /* buffer_index */, IID_PPV_ARGS(&d3d11_texture)));
  EXPECT_TRUE(d3d11_texture);
  EXPECT_EQ(d3d11_texture, back_image->texture());
  d3d11_texture.Reset();

  EXPECT_EQ(S_OK, front_image->swap_chain()->GetBuffer(
                      1 /* buffer_index */, IID_PPV_ARGS(&d3d11_texture)));
  EXPECT_TRUE(d3d11_texture);
  EXPECT_EQ(d3d11_texture, front_image->texture());
  d3d11_texture.Reset();

  GLenum target;
  GLuint service_id;
  if (uses_passthrough_texture) {
    target = back_passthrough_texture->target();
    service_id = back_passthrough_texture->service_id();
    back_passthrough_texture.reset();
  } else {
    target = back_texture->target();
    service_id = back_texture->service_id();
  }

  // Create an FBO.
  GLuint fbo = 0;
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGenFramebuffersEXTFn(1, &fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);
  api->glBindTextureFn(target, service_id);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  // Attach the texture to FBO.
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                                   service_id, 0);
  EXPECT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glViewportFn(0, 0, size.width(), size.height());
  // Set the clear color to green.
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  {
    GLubyte pixel_color[4];
    const uint8_t expected_color[4] = {0, 255, 0, 255};
    // Checks if rendering to back buffer was successful.
    api->glReadPixelsFn(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);
    EXPECT_EQ(expected_color[0], pixel_color[0]);
    EXPECT_EQ(expected_color[1], pixel_color[1]);
    EXPECT_EQ(expected_color[2], pixel_color[2]);
    EXPECT_EQ(expected_color[3], pixel_color[3]);
  }

  back_factory_ref->PresentSwapChain();

  {
    GLubyte pixel_color[4];
    const uint8_t expected_color[4] = {0, 0, 0, 255};
    // After present, back buffer should now have a clear texture.
    api->glReadPixelsFn(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);
    EXPECT_EQ(expected_color[0], pixel_color[0]);
    EXPECT_EQ(expected_color[1], pixel_color[1]);
    EXPECT_EQ(expected_color[2], pixel_color[2]);
    EXPECT_EQ(expected_color[3], pixel_color[3]);
  }

  {
    // A staging texture must be used to check front buffer since it cannot be
    // bound to an FBO or use ReadPixels.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.SampleDesc.Count = 1;

    auto d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
    ASSERT_TRUE(d3d11_device);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
    ASSERT_TRUE(SUCCEEDED(
        d3d11_device->CreateTexture2D(&desc, nullptr, &staging_texture)));

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device->GetImmediateContext(&context);
    ASSERT_TRUE(context);

    context->CopyResource(staging_texture.Get(), front_image->texture().Get());

    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    ASSERT_TRUE(SUCCEEDED(context->Map(staging_texture.Get(), 0, D3D11_MAP_READ,
                                       0, &mapped_resource)));
    ASSERT_GE(mapped_resource.RowPitch, 4u);
    // After present, front buffer should have color rendered to back buffer.
    const uint8_t* pixel_color =
        static_cast<const uint8_t*>(mapped_resource.pData);
    const uint8_t expected_color[4] = {0, 255, 0, 255};
    EXPECT_EQ(expected_color[0], pixel_color[0]);
    EXPECT_EQ(expected_color[1], pixel_color[1]);
    EXPECT_EQ(expected_color[2], pixel_color[2]);
    EXPECT_EQ(expected_color[3], pixel_color[3]);

    context->Unmap(staging_texture.Get(), 0);
  }

  api->glDeleteFramebuffersEXTFn(1, &fbo);
}

TEST_F(SwapChainFactoryDXGITest, InvalidFormat) {
  if (!SwapChainFactoryDXGI::IsSupported())
    return;
  std::unique_ptr<SwapChainFactoryDXGI> swap_chain_factory_ =
      std::make_unique<SwapChainFactoryDXGI>(false /* use_passthrough */);
  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_SCANOUT;
  {
    auto valid_format = viz::RGBA_8888;
    auto backings = swap_chain_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
    backings.front_buffer->Destroy();
    backings.back_buffer->Destroy();
  }
  {
    auto invalid_format = viz::BGRA_8888;
    auto backings = swap_chain_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, invalid_format, size,
        color_space, usage);
    EXPECT_FALSE(backings.front_buffer);
    EXPECT_FALSE(backings.back_buffer);
  }
}

TEST_F(SwapChainFactoryDXGITest, CreateAndPresentSwapChain) {
  if (!SwapChainFactoryDXGI::IsSupported() || UsesPassthrough())
    return;
  CreateAndPresentSwapChain(false /* uses_passthrough_texture */);
}

TEST_F(SwapChainFactoryDXGITest, CreateAndPresentSwapChain_PassthroughTexture) {
  if (!SwapChainFactoryDXGI::IsSupported() || !UsesPassthrough())
    return;
  CreateAndPresentSwapChain(true /* uses_passthrough_texture */);
}

}  // anonymous namespace
}  // namespace gpu
