// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_H_
#define CC_TEST_PIXEL_TEST_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "cc/test/pixel_comparator.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CopyOutputResult;
class DirectRenderer;
class DisplayResourceProvider;
class GLRenderer;
class GpuServiceImpl;
class TestSharedBitmapManager;
}

namespace cc {
class DawnSkiaRenderer;
class FakeOutputSurfaceClient;
class OutputSurface;
class VulkanSkiaRenderer;

class PixelTest : public testing::Test {
 protected:
  // Some graphics backends require command line or base::Feature initialization
  // which must occur in the constructor to avoid potential races.
  enum GraphicsBackend {
    // The pixel test will be initialized for software or GL renderers. No work
    // needs to be done in the constructor.
    kDefault,
    // SkiaRenderer with the Vulkan backend will be used.
    kSkiaVulkan,
    // SkiaRenderer with the Dawn backend will be used; on Linux this will
    // initialize Vulkan, and on Windows this will initialize D3D12.
    kSkiaDawn,
  };

  explicit PixelTest(GraphicsBackend backend = kDefault);
  ~PixelTest() override;

  bool RunPixelTest(viz::RenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTest(viz::RenderPassList* pass_list,
                    std::vector<SkColor>* ref_pixels,
                    const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTarget(viz::RenderPassList* pass_list,
                                      viz::RenderPass* target,
                                      const base::FilePath& ref_file,
                                      const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTargetAndArea(viz::RenderPassList* pass_list,
                                             viz::RenderPass* target,
                                             const base::FilePath& ref_file,
                                             const PixelComparator& comparator,
                                             const gfx::Rect* copy_rect);

  viz::ContextProvider* context_provider() const {
    return output_surface_->context_provider();
  }

  viz::GpuServiceImpl* gpu_service() {
    return gpu_service_holder_->gpu_service();
  }

  gpu::CommandBufferTaskExecutor* task_executor() {
    return gpu_service_holder_->task_executor();
  }

  // Allocates a SharedMemory bitmap and registers it with the display
  // compositor's SharedBitmapManager.
  base::WritableSharedMemoryMapping AllocateSharedBitmapMemory(
      const viz::SharedBitmapId& id,
      const gfx::Size& size);
  // Uses AllocateSharedBitmapMemory() then registers a ResourceId with the
  // |child_resource_provider_|, and copies the contents of |source| into the
  // software resource backing.
  viz::ResourceId AllocateAndFillSoftwareResource(const gfx::Size& size,
                                                  const SkBitmap& source);

  // |scoped_feature_list_| must be the first member to ensure that it is
  // destroyed after any member that might be using it.
  base::test::ScopedFeatureList scoped_feature_list_;
  viz::TestGpuServiceHolder::ScopedResetter gpu_service_resetter_;

  // For SkiaRenderer.
  viz::TestGpuServiceHolder* gpu_service_holder_ = nullptr;

  viz::RendererSettings renderer_settings_;
  gfx::Size device_viewport_size_;
  gfx::DisplayColorSpaces display_color_spaces_;
  bool disable_picture_quad_image_filtering_;
  std::unique_ptr<FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<viz::OutputSurface> output_surface_;
  std::unique_ptr<viz::TestSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<viz::DisplayResourceProvider> resource_provider_;
  scoped_refptr<viz::ContextProvider> child_context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<viz::DirectRenderer> renderer_;
  viz::SoftwareRenderer* software_renderer_ = nullptr;
  std::unique_ptr<SkBitmap> result_bitmap_;

  void SetUpGLWithoutRenderer(gfx::SurfaceOrigin output_surface_origin);
  void SetUpGLRenderer(gfx::SurfaceOrigin output_surface_origin);
  void SetUpSkiaRenderer(gfx::SurfaceOrigin output_surface_origin);
  void SetUpSoftwareRenderer();

  void TearDown() override;

  void EnableExternalStencilTest();

 private:
  void ReadbackResult(base::OnceClosure quit_run_loop,
                      std::unique_ptr<viz::CopyOutputResult> result);

  bool PixelsMatchReference(const base::FilePath& ref_file,
                            const PixelComparator& comparator);

  std::unique_ptr<gl::DisableNullDrawGLBindings> enable_pixel_output_;
};

template<typename RendererType>
class RendererPixelTest : public PixelTest {
 public:
  RendererPixelTest() : PixelTest(backend()) {}

  RendererType* renderer() {
    return static_cast<RendererType*>(renderer_.get());
  }

  // Text string for graphics backend of the RendererType. Suitable for
  // generating separate base line file paths.
  const char* renderer_type() {
    if (std::is_base_of<viz::GLRenderer, RendererType>::value)
      return "gl";
    if (std::is_base_of<DawnSkiaRenderer, RendererType>::value)
      return "dawn";
    if (std::is_base_of<viz::SkiaRenderer, RendererType>::value)
      return "skia";
    if (std::is_base_of<viz::SoftwareRenderer, RendererType>::value)
      return "software";
    return "unknown";
  }

  bool use_gpu() const { return !!child_context_provider_; }
  GraphicsBackend backend() const {
    if (std::is_base_of<VulkanSkiaRenderer, RendererType>::value)
      return kSkiaVulkan;
    if (std::is_base_of<DawnSkiaRenderer, RendererType>::value)
      return kSkiaDawn;
    return kDefault;
  }

 protected:
  void SetUp() override;
};

// Types used with gtest typed tests to specify additional behaviour, eg.
// should it be a flipped surface or what Skia backend to use.
class GLRendererWithFlippedSurface : public viz::GLRenderer {};
class SkiaRendererWithFlippedSurface : public viz::SkiaRenderer {};
class VulkanSkiaRenderer : public viz::SkiaRenderer {};
class VulkanSkiaRendererWithFlippedSurface : public VulkanSkiaRenderer {};
class DawnSkiaRenderer : public viz::SkiaRenderer {};
class DawnSkiaRendererWithFlippedSurface : public DawnSkiaRenderer {};

template <>
inline void RendererPixelTest<viz::GLRenderer>::SetUp() {
  SetUpGLRenderer(gfx::SurfaceOrigin::kBottomLeft);
}

template <>
inline void RendererPixelTest<GLRendererWithFlippedSurface>::SetUp() {
  SetUpGLRenderer(gfx::SurfaceOrigin::kTopLeft);
}

template <>
inline void RendererPixelTest<viz::SoftwareRenderer>::SetUp() {
  SetUpSoftwareRenderer();
}

template <>
inline void RendererPixelTest<viz::SkiaRenderer>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kBottomLeft);
}

template <>
inline void RendererPixelTest<SkiaRendererWithFlippedSurface>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kTopLeft);
}

template <>
inline void RendererPixelTest<VulkanSkiaRenderer>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kBottomLeft);
}

template <>
inline void RendererPixelTest<VulkanSkiaRendererWithFlippedSurface>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kTopLeft);
}

template <>
inline void RendererPixelTest<DawnSkiaRenderer>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kBottomLeft);
}

template <>
inline void RendererPixelTest<DawnSkiaRendererWithFlippedSurface>::SetUp() {
  SetUpSkiaRenderer(gfx::SurfaceOrigin::kTopLeft);
}

typedef RendererPixelTest<viz::GLRenderer> GLRendererPixelTest;
typedef RendererPixelTest<viz::SoftwareRenderer> SoftwareRendererPixelTest;

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_
