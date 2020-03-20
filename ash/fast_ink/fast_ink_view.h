// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_VIEW_H_
#define ASH_FAST_INK_FAST_INK_VIEW_H_

#include <memory>
#include <vector>

#include "ash/fast_ink/fast_ink_host.h"
#include "base/containers/flat_map.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class GpuMemoryBuffer;
}  // namespace gfx

namespace views {
class Widget;
}

namespace fast_ink {

// FastInkView is a view supporting low-latency rendering by using FastInkHost.
// THe view's widget must have the same bounds as a root window (covers the
// entire display). FastInkHost for more details.
class FastInkView : public views::View {
 public:
  // Creates a FastInkView filling the bounds of |root_window|.
  // If |root_window| is resized (e.g. due to a screen size change),
  // a new instance of FastInkView should be created.
  FastInkView(aura::Window* container,
              const FastInkHost::PresentationCallback& presentation_callback);
  ~FastInkView() override;
  FastInkView(const FastInkView&) = delete;
  FastInkView& operator=(const FastInkView&) = delete;

  // Update content and damage rectangles for surface. See
  // FastInkHost::UpdateSurface for more detials.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool auto_refresh);

 protected:
  // Helper class that provides flicker free painting to a GPU memory buffer.
  class ScopedPaint {
   public:
    ScopedPaint(FastInkView* view, const gfx::Rect& damage_rect_in_window);
    ~ScopedPaint();

    gfx::Canvas& canvas() { return canvas_; }

   private:
    gfx::GpuMemoryBuffer* const gpu_memory_buffer_;
    // Damage rect in the buffer coordinates.
    const gfx::Rect damage_rect_;
    gfx::Canvas canvas_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPaint);
  };

  FastInkHost* host() { return host_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FastInkHost> host_;
};

}  // namespace fast_ink

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
