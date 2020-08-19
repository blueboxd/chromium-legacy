// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SHM_IMAGE_POOL_H_
#define UI_BASE_X_X11_SHM_IMAGE_POOL_H_

#include <cstring>
#include <list>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/x11.h"

namespace ui {

// Creates XImages backed by shared memory that will be shared with the X11
// server for processing.
class COMPONENT_EXPORT(UI_BASE_X) XShmImagePool : public XEventDispatcher {
 public:
  XShmImagePool(x11::Connection* connection,
                x11::Drawable drawable,
                Visual* visual,
                int depth,
                std::size_t max_frames_pending);

  ~XShmImagePool() override;

  bool Resize(const gfx::Size& pixel_size);

  // Is XSHM supported by the server and are the shared buffers ready for use?
  bool Ready();

  // Obtain state for the current frame.
  SkBitmap& CurrentBitmap();
  SkCanvas* CurrentCanvas();
  XImage* CurrentImage();

  // Switch to the next cached frame.  CurrentBitmap() and CurrentImage() will
  // change to reflect the new frame.
  void SwapBuffers(base::OnceCallback<void(const gfx::Size&)> callback);

 protected:
  void DispatchShmCompletionEvent(x11::Shm::CompletionEvent event);

 private:
  friend class base::RefCountedThreadSafe<XShmImagePool>;

  struct FrameState {
    FrameState();
    ~FrameState();

    XShmSegmentInfo shminfo_{};
    bool shmem_attached_to_server_ = false;
    XScopedImage image;
    SkBitmap bitmap;
    std::unique_ptr<SkCanvas> canvas;
  };

  struct SwapClosure {
    SwapClosure();
    ~SwapClosure();

    base::OnceClosure closure;
    ShmSeg shmseg;
  };

  // XEventDispatcher:
  bool DispatchXEvent(x11::Event* xev) override;

  void Cleanup();

  x11::Connection* const connection_;
  XDisplay* const display_;
  const x11::Drawable drawable_;
  Visual* const visual_;
  const int depth_;

  bool ready_ = false;
  gfx::Size pixel_size_;
  std::size_t frame_bytes_ = 0;
  std::vector<FrameState> frame_states_;
  std::size_t current_frame_index_ = 0;
  std::list<SwapClosure> swap_closures_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(XShmImagePool);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SHM_IMAGE_POOL_H_
