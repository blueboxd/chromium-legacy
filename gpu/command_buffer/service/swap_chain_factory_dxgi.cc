// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/swap_chain_factory_dxgi.h"

#include <d3d11.h>

#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_image_dxgi_swap_chain.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

GLuint MakeTextureAndSetParameters(gl::GLApi* api, GLenum target) {
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                         GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  return service_id;
}

bool ClearBackBuffer(Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
                     Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex << hr;
    return false;
  }
  DCHECK(d3d11_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
  hr = d3d11_device->CreateRenderTargetView(d3d11_texture.Get(), nullptr,
                                            &render_target);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateRenderTargetView failed with error " << std::hex
                << hr;
    return false;
  }
  DCHECK(render_target);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(&d3d11_device_context);
  DCHECK(d3d11_device_context);

  float color_rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  d3d11_device_context->ClearRenderTargetView(render_target.Get(), color_rgba);
  return true;
}

bool SupportedFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_F16:
    case viz::RGBA_8888:
    case viz::RGBX_8888:
      return true;
    default:
      return false;
  };
}

}  // anonymous namespace

// Representation of a SharedImageBackingDXGISwapChain as a GL Texture.
class SharedImageRepresentationGLTextureDXGISwapChain
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureDXGISwapChain(SharedImageManager* manager,
                                                  SharedImageBacking* backing,
                                                  MemoryTypeTracker* tracker,
                                                  gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

 private:
  gles2::Texture* const texture_;
};

// Representation of a SharedImageBackingDXGISwapChain as a GL
// TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughDXGISwapChain
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughDXGISwapChain(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_passthrough_(std::move(texture_passthrough)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_passthrough_;
  }

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

// Implementation of SharedImageBacking that holds buffer (front buffer/back
// buffer of swap chain) texture (as gles2::Texture/gles2::TexturePassthrough)
// and a reference to created swap chain.
class SharedImageBackingDXGISwapChain : public SharedImageBacking {
 public:
  SharedImageBackingDXGISwapChain(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      gles2::Texture* texture,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           texture ? texture->estimated_size()
                                   : passthrough_texture->estimated_size(),
                           false /* is_thread_safe */),
        swap_chain_(swap_chain),
        texture_(texture),
        texture_passthrough_(std::move(passthrough_texture)) {
    DCHECK(swap_chain_);
    DCHECK((texture_ && !texture_passthrough_) ||
           (!texture_ && texture_passthrough_));
  }

  ~SharedImageBackingDXGISwapChain() override {
    DCHECK(!swap_chain_);
    DCHECK(!texture_);
    DCHECK(!texture_passthrough_);
  }

  // Texture is cleared on initialization.
  bool IsCleared() const override { return true; }

  void SetCleared() override {}

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    DLOG(ERROR) << "SharedImageBackingDXGISwapChain::Update : Trying to update "
                   "Shared Images associated with swap chain.";
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    if (texture_)
      mailbox_manager->ProduceTexture(mailbox(), texture_);
    else {
      DCHECK(texture_passthrough_);
      mailbox_manager->ProduceTexture(mailbox(), texture_passthrough_.get());
    }
    return true;
  }

  void Destroy() override {
    if (texture_) {
      texture_->RemoveLightweightRef(have_context());
      texture_ = nullptr;
    } else {
      DCHECK(texture_passthrough_);
      if (!have_context())
        texture_passthrough_->MarkContextLost();
      texture_passthrough_.reset();
    }

    DCHECK(swap_chain_);
    swap_chain_.Reset();
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    base::trace_event::MemoryAllocatorDumpGuid service_guid;
    if (texture_)
      service_guid =
          gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    else {
      DCHECK(texture_passthrough_);
      service_guid = gl::GetGLTextureServiceGUIDForTracing(
          texture_passthrough_->service_id());
    }
    pmd->CreateSharedGlobalAllocatorDump(service_guid);

    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.

    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    // Dump all sub-levels held by the texture. They will appear below the
    // main gl/textures/client_X/mailbox_Y dump.
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }

  bool PresentSwapChain() override {
    DXGI_PRESENT_PARAMETERS params = {};
    params.DirtyRectsCount = 0;
    params.pDirtyRects = nullptr;
    HRESULT hr =
        swap_chain_->Present1(0 /* interval */, 0 /* flags */, &params);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
      return false;
    }

    gl::GLImage* image;
    unsigned target = GL_TEXTURE_2D;
    if (texture_) {
      gles2::Texture::ImageState image_state;
      image = texture_->GetLevelImage(target, 0, &image_state);
      DCHECK_EQ(image_state, gles2::Texture::BOUND);
    } else {
      DCHECK(texture_passthrough_);
      image = texture_passthrough_->GetLevelImage(target, 0);
    }
    DCHECK(image);

    if (!image->BindTexImage(target)) {
      DLOG(ERROR) << "Failed to rebind texture to new surface.";
      return false;
    }
    return true;
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    DCHECK(texture_);
    return std::make_unique<SharedImageRepresentationGLTextureDXGISwapChain>(
        manager, this, tracker, texture_);
  }

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    DCHECK(texture_passthrough_);
    return std::make_unique<
        SharedImageRepresentationGLTexturePassthroughDXGISwapChain>(
        manager, this, tracker, texture_passthrough_);
  }

 private:
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingDXGISwapChain);
};

SwapChainFactoryDXGI::SwapChainFactoryDXGI(bool use_passthrough)
    : use_passthrough_(use_passthrough) {}

SwapChainFactoryDXGI::~SwapChainFactoryDXGI() = default;

SwapChainFactoryDXGI::SwapChainBackings::SwapChainBackings(
    std::unique_ptr<SharedImageBacking> front_buffer,
    std::unique_ptr<SharedImageBacking> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}

SwapChainFactoryDXGI::SwapChainBackings::~SwapChainBackings() = default;

SwapChainFactoryDXGI::SwapChainBackings::SwapChainBackings(
    SwapChainFactoryDXGI::SwapChainBackings&&) = default;

SwapChainFactoryDXGI::SwapChainBackings&
SwapChainFactoryDXGI::SwapChainBackings::operator=(
    SwapChainFactoryDXGI::SwapChainBackings&&) = default;

// static
bool SwapChainFactoryDXGI::IsSupported() {
  return gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported();
}

std::unique_ptr<SharedImageBacking> SwapChainFactoryDXGI::MakeBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
    int buffer_index) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr =
      swap_chain->GetBuffer(buffer_index, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex << hr;
    return nullptr;
  }
  DCHECK(d3d11_texture);

  const unsigned target = GL_TEXTURE_2D;
  gl::GLApi* api = gl::g_current_gl_context;
  const GLuint service_id = MakeTextureAndSetParameters(api, target);

  auto image = base::MakeRefCounted<gl::GLImageDXGISwapChain>(
      size, viz::BufferFormat(format), d3d11_texture, swap_chain);
  if (!image->Initialize()) {
    DLOG(ERROR) << "Failed to create EGL image";
    return nullptr;
  }
  if (!image->BindTexImage(target)) {
    DLOG(ERROR) << "Failed to bind image to swap chain D3D11 texture.";
    return nullptr;
  }

  gles2::Texture* texture = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture;

  if (use_passthrough_) {
    passthrough_texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
    passthrough_texture->SetLevelImage(target, 0, image.get());
    GLint texture_memory_size = 0;
    api->glGetTexParameterivFn(target, GL_MEMORY_SIZE_ANGLE,
                               &texture_memory_size);
    passthrough_texture->SetEstimatedSize(texture_memory_size);
  } else {
    GLuint internal_format = viz::GLInternalFormat(format);
    DCHECK_EQ(internal_format, image->GetInternalFormat());
    GLenum gl_format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(internal_format);
    GLenum gl_type =
        gles2::TextureManager::ExtractTypeFromStorageFormat(internal_format);

    texture = new gles2::Texture(service_id);
    texture->SetLightweightRef();
    texture->SetTarget(target, 1);
    texture->sampler_state_.min_filter = GL_LINEAR;
    texture->sampler_state_.mag_filter = GL_LINEAR;
    texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture->SetLevelInfo(target, 0, internal_format, size.width(),
                          size.height(), 1, 0, gl_format, gl_type,
                          gfx::Rect(size));
    texture->SetLevelImage(target, 0, image.get(), gles2::Texture::BOUND);
    texture->SetImmutable(true, false);
  }

  return std::make_unique<SharedImageBackingDXGISwapChain>(
      mailbox, format, size, color_space, usage, swap_chain, texture,
      passthrough_texture);
}

SwapChainFactoryDXGI::SwapChainBackings SwapChainFactoryDXGI::CreateSwapChain(
    const Mailbox& front_buffer_mailbox,
    const Mailbox& back_buffer_mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (!SupportedFormat(format)) {
    DLOG(ERROR) << format << " format is not supported by swap chain.";
    return {nullptr, nullptr};
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  DCHECK(d3d11_device);
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(dxgi_factory);

  DXGI_FORMAT output_format = format == viz::RGBA_F16
                                  ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                  : DXGI_FORMAT_B8G8R8A8_UNORM;

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.Format = output_format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = 0;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

  HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
      d3d11_device.Get(), &desc, nullptr, &swap_chain);

  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateSwapChainForComposition failed with error "
                << std::hex << hr;
    return {nullptr, nullptr};
  }

  // Explicitly clear front and back buffers to ensure that there are no
  // uninitialized pixels.
  if (!ClearBackBuffer(swap_chain, d3d11_device))
    return {nullptr, nullptr};
  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  swap_chain->Present1(0 /* interval */, 0 /* flags */, &params);
  if (!ClearBackBuffer(swap_chain, d3d11_device))
    return {nullptr, nullptr};

  std::unique_ptr<SharedImageBacking> front_buffer_backing =
      MakeBacking(front_buffer_mailbox, format, size, color_space, usage,
                  swap_chain, 1 /* buffer index */);

  std::unique_ptr<SharedImageBacking> back_buffer_backing =
      MakeBacking(back_buffer_mailbox, format, size, color_space, usage,
                  swap_chain, 0 /* buffer index */);

  // To avoid registering one backing when the other backing does not exist.
  if (!(front_buffer_backing && back_buffer_backing))
    return {nullptr, nullptr};

  return {std::move(front_buffer_backing), std::move(back_buffer_backing)};
}

}  // namespace gpu
