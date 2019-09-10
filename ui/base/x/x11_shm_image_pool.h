// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SHM_IMAGE_POOL_H_
#define UI_BASE_X_X11_SHM_IMAGE_POOL_H_

#include <cstring>
#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_X) XShmImagePool
    : public PlatformEventDispatcher,
      public base::RefCountedThreadSafe<XShmImagePool> {
 public:
  XShmImagePool(base::TaskRunner* host_task_runner,
                base::TaskRunner* event_task_runner,
                XDisplay* display,
                XID drawable,
                Visual* visual,
                int depth,
                std::size_t max_frames_pending);

  bool Resize(const gfx::Size& pixel_size);

  // Is XSHM supported by the server and are the shared buffers ready for use?
  bool Ready();

  // Obtain state for the current frame.
  SkBitmap& CurrentBitmap();
  XImage* CurrentImage();

  // Switch to the next cached frame.  CurrentBitmap() and CurrentImage() will
  // change to reflect the new frame.
  void SwapBuffers(base::OnceCallback<void(const gfx::Size&)> callback);

  // Part of setup and teardown must be done on the event task runner.  Posting
  // the tasks cannot be done in the constructor/destructor because because this
  // would cause subtle problems with the reference count for this object.  So
  // Initialize() must be called after constructing and Teardown() must be
  // called before destructing.
  void Initialize();
  void Teardown();

 private:
  friend class base::RefCountedThreadSafe<XShmImagePool>;

  struct FrameState {
    FrameState();
    ~FrameState();

    XScopedImage image;
    SkBitmap bitmap;
#ifndef NDEBUG
    std::size_t offset = 0;
#endif
  };

  struct SwapClosure {
    SwapClosure();
    ~SwapClosure();

    base::OnceClosure closure;
#ifndef NDEBUG
    ShmSeg shmseg;
    std::size_t offset;
#endif
  };

  ~XShmImagePool() override;

  void InitializeOnGpu();
  void TeardownOnGpu();

  void Cleanup();

  void DispatchShmCompletionEvent(XShmCompletionEvent event);

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  base::TaskRunner* const host_task_runner_;
  base::TaskRunner* const event_task_runner_;

  XDisplay* const display_;
  const XID drawable_;
  Visual* const visual_;
  const int depth_;

  gfx::Size pixel_size_;
  std::size_t frame_bytes_ = 0;
  XShmSegmentInfo shminfo_{};
  bool shmem_attached_to_server_ = false;
  std::vector<FrameState> frame_states_;
  std::size_t current_frame_index_ = 0;
  std::queue<SwapClosure> swap_closures_;

#ifndef NDEBUG
  bool dispatcher_registered_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(XShmImagePool);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SHM_IMAGE_POOL_H_
