// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gl {
class SurfaceTexture;
}  // namespace gl

namespace gfx {
class ColorSpace;
}

namespace gpu {
class ContextSupport;
class GpuMemoryBufferImplAndroidHardwareBuffer;
struct MailboxHolder;
struct SyncToken;
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
}

namespace vr {

class MailboxToSurfaceBridge {
 public:
  MailboxToSurfaceBridge();
  virtual ~MailboxToSurfaceBridge();

  // Returns true if the GPU process connection is established and ready to use.
  // Equivalent to waiting for on_initialized to be called.
  virtual bool IsConnected();

  // Checks if a workaround from "gpu/config/gpu_driver_bug_workaround_type.h"
  // is active. Requires initialization to be complete.
  bool IsGpuWorkaroundEnabled(int32_t workaround);

  void CreateSurface(gl::SurfaceTexture*);

  // This class can be used in a couple ways using these sequences:
  //
  // To use entirely on the GL thread:
  // Call CreateAndBindContextProvider(callback) from your thread.
  // When the callback is invoked, the object is ready for calls that use the
  // context, such as CreateSharedImage().
  //
  // To create on one thread and use GL on another:
  // Call CreateUnboundContextProvider(callback) and then make sure
  // to call BindContextProviderToCurrentThread() from your GL
  // thread afterwards before making a context-related calls.

  // Asynchronously create the context using the surface provided by an earlier
  // CreateSurface call, or an offscreen context if that wasn't called. Also
  // binds the context provider to the thread used for constructing the
  // MailboxToSurfaceBridge object, and calls the callback on the constructor
  // thread. Use this if constructing the object on the intended GL thread.
  void CreateAndBindContextProvider(base::OnceClosure callback);

  // Variant of above, use this if the MailboxToSurfaceBridge constructor
  // wasn't run on the GL thread. The provided callback is run on the
  // constructor thread. After that, you can pass the MailboxToSurfaceBridge
  // to another thread. You must call BindContextProviderToCurrentThread()
  // on the target GL thread before using any context-related methods.
  // The context-related methods check that they are called on this thread, so
  // there will be a DCHECK error if they are not used consistently.
  virtual void CreateUnboundContextProvider(base::OnceClosure callback);

  // Client must call this on the target (GL) thread after
  // CreateUnboundContextProvider. It's called automatically when using
  // CreateAndBindContextProvider.
  virtual void BindContextProviderToCurrentThread();

  // All other public methods below must be called on the GL thread
  // (except when marked otherwise).

  void ResizeSurface(int width, int height);

  // Returns true if swapped successfully. This can fail if the GL
  // context isn't ready for use yet, in that case the caller
  // won't get a new frame on the SurfaceTexture.
  bool CopyMailboxToSurfaceAndSwap(const gpu::MailboxHolder& mailbox);

  void GenSyncToken(gpu::SyncToken* out_sync_token);

  void WaitSyncToken(const gpu::SyncToken& sync_token);

  // Copies a GpuFence from the local context to the GPU process,
  // and issues a server wait for it.
  void WaitForClientGpuFence(gfx::GpuFence*);

  // Creates a GpuFence in the GPU process after the supplied sync_token
  // completes, and copies it for use in the local context. This is
  // asynchronous, the callback receives the GpuFence once it's available.
  void CreateGpuFence(
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback);

  // Creates a shared image bound to |buffer|. Returns a mailbox holder that
  // references the shared image with a sync token representing a point after
  // the creation. Caller must call DestroySharedImage to free the shared image.
  // Does not take ownership of |buffer| or retain any references to it.
  gpu::MailboxHolder CreateSharedImage(
      gpu::GpuMemoryBufferImplAndroidHardwareBuffer* buffer,
      const gfx::ColorSpace& color_space,
      uint32_t usage);

  // Destroys a shared image created by CreateSharedImage. The mailbox_holder's
  // sync_token must have been updated to a sync token after the last use of the
  // shared image.
  void DestroySharedImage(const gpu::MailboxHolder& mailbox_holder);

 private:
  void CreateContextProviderInternal();
  void OnContextAvailableOnUiThread(
      scoped_refptr<viz::ContextProvider> provider);
  void InitializeRenderer();
  void DestroyContext();
  void DrawQuad(unsigned int textureHandle);

  scoped_refptr<viz::ContextProvider> context_provider_;
  std::unique_ptr<gl::ScopedJavaSurface> surface_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  gpu::ContextSupport* context_support_ = nullptr;
  int surface_handle_ = gpu::kNullSurfaceHandle;
  // TODO(https://crbug.com/836524): shouldn't have both of these closures
  // in the same class like this.
  base::OnceClosure on_context_bound_;
  base::OnceClosure on_context_provider_ready_;

  // Saved state for a pending resize, the dimensions are only
  // valid if needs_resize_ is true.
  bool needs_resize_ = false;
  int resize_width_;
  int resize_height_;

  // A swap ID which is passed to GL swap. Incremented each call.
  uint64_t swap_id_ = 0;

  // A task runner for the thread the object was created on.
  scoped_refptr<base::SingleThreadTaskRunner> constructor_thread_task_runner_;

  // Must be last.
  base::WeakPtrFactory<MailboxToSurfaceBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MailboxToSurfaceBridge);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
