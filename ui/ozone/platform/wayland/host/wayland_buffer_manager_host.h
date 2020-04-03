// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// This is an internal helper representation of a wayland buffer object, which
// the GPU process creates when CreateBuffer is called. It's used for
// asynchronous buffer creation and stores |params| parameter to find out,
// which Buffer the wl_buffer corresponds to when CreateSucceeded is called.
// What is more, the Buffer stores such information as a widget it is attached
// to, its buffer id for simpler buffer management and other members specific
// to this Buffer object on run-time.
struct WaylandBuffer {
  WaylandBuffer() = delete;
  WaylandBuffer(const gfx::Size& size, uint32_t buffer_id);
  ~WaylandBuffer();

  // Actual buffer size.
  const gfx::Size size;

  // Damage region this buffer describes. Must be emptied once buffer is
  // submitted.
  gfx::Rect damage_region;

  // The id of this buffer.
  const uint32_t buffer_id;

  // A wl_buffer backed by a dmabuf created on the GPU side.
  wl::Object<struct wl_buffer> wl_buffer;

  // Tells if the buffer has the wl_buffer attached. This can be used to
  // identify potential problems, when the Wayland compositor fails to create
  // wl_buffers.
  bool attached = false;

  // Tells if the buffer has already been released aka not busy, and the
  // surface can tell the gpu about successful swap.
  bool released = true;

  DISALLOW_COPY_AND_ASSIGN(WaylandBuffer);
};

// This is the buffer manager which creates wl_buffers based on dmabuf (hw
// accelerated compositing) or shared memory (software compositing) and uses
// internal representation of surfaces, which are used to store buffers
// associated with the WaylandWindow.
class WaylandBufferManagerHost : public WaylandWindowObserver {
 public:
  WaylandBufferManagerHost();
  ~WaylandBufferManagerHost() override;

  void SetWaylandConnection(WaylandConnection* connection);

  // WaylandWindowObserver implements:
  void OnWindowAdded(WaylandWindow* window) override;
  void OnWindowRemoved(WaylandWindow* window) override;

  // Returns supported buffer formats either from zwp_linux_dmabuf or wl_drm.
  wl::BufferFormatsWithModifiersMap GetSupportedBufferFormats() const;

  bool SupportsDmabuf() const;

  // Creates a wl_buffer based on a gbm file descriptor using zwp_linux_dmabuf
  // protocol.
  void CreateBufferDmabuf(base::ScopedFD dmabuf_fd,
                          const gfx::Size& size,
                          const std::vector<uint32_t>& strides,
                          const std::vector<uint32_t>& offsets,
                          const std::vector<uint64_t>& modifiers,
                          uint32_t format,
                          uint32_t planes_count,
                          uint32_t buffer_id);

  // Creates a wl_buffer based on a shared memory file descriptor using wl_shm
  // protocol.
  void CreateBufferShm(base::ScopedFD shm_fd,
                       uint64_t length,
                       const gfx::Size& size,
                       uint32_t buffer_id);

  // Called by the GPU to destroy the imported wl_buffer with a |buffer_id|.
  void DestroyBufferWithId(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // Attaches a wl_buffer with a |buffer_id| to a
  // WaylandWindow with the specified |widget|.
  // Calls OnSubmission and OnPresentation on successful swap and pixels
  // presented.
  void CommitBufferWithId(gfx::AcceleratedWidget widget,
                          uint32_t buffer_id,
                          const gfx::Rect& damage_region);

  // When a surface is hidden, the client may want to detach the buffer attached
  // to the surface backed by |widget| to ensure Wayland does not present those
  // contents and do not composite in a wrong way. Otherwise, users may see the
  // contents of a hidden surface on their screens.
  void ResetSurfaceContents(gfx::AcceleratedWidget widget);

  // Returns the anonymously created WaylandBuffer.
  std::unique_ptr<WaylandBuffer> PassAnonymousWlBuffer(uint32_t buffer_id);

 protected:
  // Clears the state of the |buffer_manager_|.
  void ClearInternalState();

 private:
  // This is an internal representation of a real surface, which holds a pointer
  // to WaylandWindow. Also, this object holds buffers, frame callbacks and
  // presentation callbacks for that window's surface.
  class Surface;

  bool CreateBuffer(const gfx::Size& size, uint32_t buffer_id);

  Surface* GetSurface(gfx::AcceleratedWidget widget) const;

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const base::ScopedFD& file,
                           const gfx::Size& size,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t format,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateBufferIdFromGpu(uint32_t buffer_id);
  bool ValidateDataFromGpu(const base::ScopedFD& file,
                           size_t length,
                           const gfx::Size& size,
                           uint32_t buffer_id);

  // Callback method. Receives a result for the request to create a wl_buffer
  // backend by dmabuf file descriptor from ::CreateBuffer call.
  void OnCreateBufferComplete(uint32_t buffer_id,
                              wl::Object<struct wl_buffer> new_buffer);

  // Notifies about the swap result and the presentation feedback.
  virtual void OnSubmission(gfx::AcceleratedWidget widget,
                            uint32_t buffer_id,
                            const gfx::SwapResult& swap_result) = 0;
  virtual void OnPresentation(gfx::AcceleratedWidget widget,
                              uint32_t buffer_id,
                              const gfx::PresentationFeedback& feedback) = 0;

  // Notifies invalid data has been received.
  virtual void OnError(std::string error_message) = 0;

  bool DestroyAnonymousBuffer(uint32_t buffer_id);

  base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Surface>> surfaces_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  WaylandConnection* connection_ = nullptr;

  // Contains anonymous buffers aka buffers that are not attached to any of the
  // existing surfaces and that will be mapped to surfaces later.  Typically
  // created when CreateAnonymousImage is called on the gpu process side.
  base::flat_map<uint32_t, std::unique_ptr<WaylandBuffer>> anonymous_buffers_;

  base::WeakPtrFactory<WaylandBufferManagerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
