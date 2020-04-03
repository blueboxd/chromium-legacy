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
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

#if defined(WAYLAND_GBM)
#include "ui/gfx/linux/gbm_device.h"  // nogncheck
#endif

namespace gfx {
enum class SwapResult;
class Rect;
}  // namespace gfx

namespace ui {

class WaylandSurfaceGpu;
class WaylandWindow;

// Base implementation of WaylandBufferManagerGpu.
class WaylandBufferManagerGpu {
 public:
  WaylandBufferManagerGpu();
  virtual ~WaylandBufferManagerGpu();

  // Stores supported buffer formats with modifiers. If |supports_dma_buf|,
  // |gbm_device_| will be reset, which means dmabuf based buffers can't be
  // created.
  void StoreBufferFormatsWithModifiers(
      const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
          buffer_formats_with_modifiers,
      bool supports_dma_buf);

  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new buffer for swap.
  void OnBufferSubmitted(gfx::AcceleratedWidget widget,
                         uint32_t buffer_id,
                         gfx::SwapResult swap_result);
  void OnBufferPresented(gfx::AcceleratedWidget widget,
                         uint32_t buffer_id,
                         const gfx::PresentationFeedback& feedback);

  // If the client, which uses this manager and implements WaylandSurfaceGpu,
  // wants to receive OnSubmission and OnPresentation callbacks and know the
  // result of the below operations, they must register themselves with the
  // below APIs.
  void RegisterSurface(gfx::AcceleratedWidget widget,
                       WaylandSurfaceGpu* surface);
  void UnregisterSurface(gfx::AcceleratedWidget widget);
  WaylandSurfaceGpu* GetSurface(gfx::AcceleratedWidget widget);

  // Asks Wayland to create generic dmabuf-based wl_buffer.
  virtual void CreateDmabufBasedBuffer(base::ScopedFD dmabuf_fd,
                                       gfx::Size size,
                                       const std::vector<uint32_t>& strides,
                                       const std::vector<uint32_t>& offsets,
                                       const std::vector<uint64_t>& modifiers,
                                       uint32_t current_format,
                                       uint32_t planes_count,
                                       uint32_t buffer_id) = 0;

  // Asks Wayland to create a shared memory based wl_buffer.
  virtual void CreateShmBasedBuffer(base::ScopedFD shm_fd,
                                    size_t length,
                                    gfx::Size size,
                                    uint32_t buffer_id) = 0;

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
  virtual void CommitBuffer(gfx::AcceleratedWidget widget,
                            uint32_t buffer_id,
                            const gfx::Rect& damage_region) = 0;

  // Asks Wayland to destroy a wl_buffer.
  virtual void DestroyBuffer(gfx::AcceleratedWidget widget,
                             uint32_t buffer_id) = 0;

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* gbm_device() const { return gbm_device_.get(); }
  void set_gbm_device(std::unique_ptr<GbmDevice> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }
#endif

  // Returns supported modifiers for the supplied |buffer_format|.
  const std::vector<uint64_t>& GetModifiersForBufferFormat(
      gfx::BufferFormat buffer_format) const;

  // Allocates a unique buffer ID.
  uint32_t AllocateBufferID();

 private:
#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif

  std::map<gfx::AcceleratedWidget, WaylandSurfaceGpu*>
      widget_to_surface_map_;  // Guarded by |lock_|.

  // Supported buffer formats and modifiers sent by the Wayland compositor to
  // the client. Corresponds to the map stored in WaylandZwpLinuxDmabuf and
  // passed from it during initialization of this gpu host.
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
      supported_buffer_formats_with_modifiers_;

  // Protects access to |widget_to_surface_map_|.
  base::Lock lock_;

  // Keeps track of the next unique buffer ID.
  uint32_t next_buffer_id_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
