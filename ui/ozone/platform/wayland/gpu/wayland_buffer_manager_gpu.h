// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_

#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/common/linux/gbm_device.h"  // nogncheck
#endif

namespace gfx {
enum class SwapResult;
class Rect;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandSurfaceGpu;
class WaylandWindow;

// Forwards calls through an associated mojo connection to WaylandBufferManager
// on the browser process side.
//
// It's guaranteed that WaylandBufferManagerGpu makes mojo calls on the right
// sequence.
class WaylandBufferManagerGpu : public ozone::mojom::WaylandBufferManagerGpu {
 public:
  using BufferManagerHostPtr = ozone::mojom::WaylandBufferManagerHostPtr;

  WaylandBufferManagerGpu();
  ~WaylandBufferManagerGpu() override;

  // WaylandBufferManagerGpu overrides:
  void SetWaylandBufferManagerHost(
      BufferManagerHostPtr buffer_manager_host_ptr) override;

  void ResetGbmDevice() override;

  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new buffer for swap.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    gfx::SwapResult swap_result) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  // If the client, which uses this manager and implements WaylandSurfaceGpu,
  // wants to receive OnSubmission and OnPresentation callbacks and know the
  // result of the below operations, they must register themselves with the
  // below APIs.
  void RegisterSurface(gfx::AcceleratedWidget widget,
                       WaylandSurfaceGpu* surface);
  void UnregisterSurface(gfx::AcceleratedWidget widget);
  WaylandSurfaceGpu* GetSurface(gfx::AcceleratedWidget widget);

  // Methods, which can be used when in both in-process-gpu and out of process
  // modes. These calls are forwarded to the browser process through the
  // WaylandConnection mojo interface. See more in
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  //
  // Asks Wayland to create generic dmabuf-based wl_buffer.
  void CreateDmabufBasedBuffer(gfx::AcceleratedWidget widget,
                               base::ScopedFD dmabuf_fd,
                               gfx::Size size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t current_format,
                               uint32_t planes_count,
                               uint32_t buffer_id);

  // Asks Wayland to create a shared memory based wl_buffer.
  void CreateShmBasedBuffer(gfx::AcceleratedWidget widget,
                            base::ScopedFD shm_fd,
                            size_t length,
                            gfx::Size size,
                            uint32_t buffer_id);

  // Asks Wayland to find a wl_buffer with the |buffer_id| and attach the
  // buffer to the WaylandWindow's surface, which backs the following |widget|.
  // Once the buffer is submitted and presented, the OnSubmission and
  // OnPresentation are called. Note, it's not guaranteed the OnPresentation
  // will follow the OnSubmission immediately, but the OnPresentation must never
  // be called before the OnSubmission is called for that particular buffer.
  // This logic must be checked by the client, though the host ensures this
  // logic as well. This call must not be done twice for the same |widget| until
  // the OnSubmission is called (which actually means the client can continue
  // sending buffer swap requests).
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region);

  // Asks Wayland to destroy a wl_buffer.
  void DestroyBuffer(gfx::AcceleratedWidget widget, uint32_t buffer_id);

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* gbm_device() const { return gbm_device_.get(); }
  void set_gbm_device(std::unique_ptr<GbmDevice> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }
#endif

  // Adds a WaylandBufferManagerGpu binding.
  void AddBindingWaylandBufferManagerGpu(
      ozone::mojom::WaylandBufferManagerGpuRequest request);

 private:
  void CreateDmabufBasedBufferInternal(gfx::AcceleratedWidget widget,
                                       base::ScopedFD dmabuf_fd,
                                       gfx::Size size,
                                       const std::vector<uint32_t>& strides,
                                       const std::vector<uint32_t>& offsets,
                                       const std::vector<uint64_t>& modifiers,
                                       uint32_t current_format,
                                       uint32_t planes_count,
                                       uint32_t buffer_id);
  void CreateShmBasedBufferInternal(gfx::AcceleratedWidget widget,
                                    base::ScopedFD shm_fd,
                                    size_t length,
                                    gfx::Size size,
                                    uint32_t buffer_id);
  void CommitBufferInternal(gfx::AcceleratedWidget widget,
                            uint32_t buffer_id,
                            const gfx::Rect& damage_region);
  void DestroyBufferInternal(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  void BindHostInterface(BufferManagerHostPtr buffer_manager_host_ptr);

  // Provides the WaylandSurfaceGpu, which backs the |widget|, with swap and
  // presentation results.
  void SubmitSwapResultOnOriginThread(gfx::AcceleratedWidget widget,
                                      uint32_t buffer_id,
                                      gfx::SwapResult swap_result);
  void SubmitPresentationtOnOriginThread(
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      const gfx::PresentationFeedback& feedback);

#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif

  mojo::BindingSet<ozone::mojom::WaylandBufferManagerGpu> bindings_;

  // A pointer to a WaylandBufferManagerHost object, which always lives on a
  // browser process side. It's used for a multi-process mode.
  BufferManagerHostPtr buffer_manager_host_ptr_;

  mojo::AssociatedBinding<ozone::mojom::WaylandBufferManagerGpu>
      associated_binding_;

  std::map<gfx::AcceleratedWidget, WaylandSurfaceGpu*>
      widget_to_surface_map_;  // Guarded by |lock_|.

  // This task runner can be used to pass messages back to the same thread,
  // where the commit buffer request came from. For example, swap requests come
  // from the GpuMainThread, but rerouted to the IOChildThread and then mojo
  // calls happen. However, when the manager receives mojo calls, it has to
  // reroute calls back to the same thread where the calls came from to ensure
  // correct sequence.
  scoped_refptr<base::SingleThreadTaskRunner> commit_thread_runner_;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on IOChildThread. This is
  // needed to ensure mojo calls happen on a right sequence.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;

  // Protects access to |widget_to_surface_map_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerGpu);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
