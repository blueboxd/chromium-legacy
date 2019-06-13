// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/logging.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#endif

namespace gpu {

namespace {

struct FlushCleanupContext {
  std::vector<base::OnceClosure> cleanup_tasks;
};

void CleanupAfterSkiaFlush(void* context) {
  FlushCleanupContext* flush_context =
      static_cast<FlushCleanupContext*>(context);
  for (auto& task : flush_context->cleanup_tasks) {
    std::move(task).Run();
  }
  delete flush_context;
}

}  // namespace

bool GetGrBackendTexture(const gl::GLVersionInfo* version_info,
                         GLenum target,
                         const gfx::Size& size,
                         GLuint service_id,
                         viz::ResourceFormat resource_format,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB &&
      target != GL_TEXTURE_EXTERNAL_OES) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;
  texture_info.fFormat = gl::GetInternalFormat(
      version_info, viz::TextureStorageFormat(resource_format));
  *gr_texture = GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                                 texture_info);
  return true;
}

void AddCleanupTaskForSkiaFlush(base::OnceClosure task,
                                GrFlushInfo* flush_info) {
  FlushCleanupContext* context;
  if (!flush_info->fFinishedProc) {
    DCHECK(!flush_info->fFinishedContext);
    flush_info->fFinishedProc = &CleanupAfterSkiaFlush;
    context = new FlushCleanupContext();
    flush_info->fFinishedContext = context;
  } else {
    DCHECK_EQ(flush_info->fFinishedProc, &CleanupAfterSkiaFlush);
    DCHECK(flush_info->fFinishedContext);
    context = static_cast<FlushCleanupContext*>(flush_info->fFinishedContext);
  }
  context->cleanup_tasks.push_back(std::move(task));
}

void AddVulkanCleanupTaskForSkiaFlush(
    viz::VulkanContextProvider* context_provider,
    GrFlushInfo* flush_info) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (context_provider) {
    auto task = context_provider->GetDeviceQueue()
                    ->GetFenceHelper()
                    ->CreateExternalCallback();
    if (task)
      AddCleanupTaskForSkiaFlush(std::move(task), flush_info);
  }
#endif
}

void DeleteGrBackendTexture(SharedContextState* context_state,
                            GrBackendTexture* backend_texture) {
  DCHECK(backend_texture && backend_texture->isValid());
  if (!context_state->GrContextIsVulkan()) {
    context_state->gr_context()->deleteBackendTexture(
        std::move(*backend_texture));
    return;
  }

#if BUILDFLAG(ENABLE_VULKAN)
  auto* fence_helper =
      context_state->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](const sk_sp<GrContext>& gr_context, GrBackendTexture backend_texture,
         gpu::VulkanDeviceQueue* device_queue, bool is_lost) {
        // If underlying Vulkan device is destroyed, gr_context should have been
        // abandoned, the deleteBackendTexture() should be noop.
        gr_context->deleteBackendTexture(std::move(backend_texture));
      },
      sk_ref_sp(context_state->gr_context()), std::move(*backend_texture)));
#endif
}

}  // namespace gpu
