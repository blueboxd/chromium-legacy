// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/linux_output_window.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "ui/base/x/x11_software_bitmap_presenter.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/xproto.h"

namespace media {
namespace cast {
namespace test {

LinuxOutputWindow::LinuxOutputWindow(int x_pos,
                                     int y_pos,
                                     int width,
                                     int height,
                                     const std::string& name) {
  CreateWindow(x_pos, y_pos, width, height, name);
}

LinuxOutputWindow::~LinuxOutputWindow() {
  if (window_ != x11::Window::None)
    connection_.DestroyWindow({window_});
  connection_.Flush();
}

void LinuxOutputWindow::CreateWindow(int x_pos,
                                     int y_pos,
                                     int width,
                                     int height,
                                     const std::string& name) {
  if (!connection_.Ready()) {
    // There's no point to continue if this happens: nothing will work anyway.
    VLOG(1) << "Failed to connect to X server: X environment likely broken";
    NOTREACHED();
  }

  x11::VisualId visual;
  uint8_t depth;
  x11::ColorMap colormap;
  ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
      false, &visual, &depth, &colormap, nullptr);
  window_ = connection_.GenerateId<x11::Window>();
  connection_.CreateWindow({
      .depth = depth,
      .wid = window_,
      .parent = connection_.default_root(),
      .x = x_pos,
      .y = y_pos,
      .width = width,
      .height = height,
      .border_width = 0,
      .c_class = x11::WindowClass::InputOutput,
      .visual = visual,
      .background_pixel = 0,
      .border_pixel = 0,
      .event_mask = x11::EventMask::StructureNotify,
      .colormap = colormap,
  });

  // Set window name.
  ui::SetStringProperty(window_, x11::Atom::WM_NAME, x11::Atom::STRING, name);
  ui::SetStringProperty(window_, x11::Atom::WM_ICON_NAME, x11::Atom::STRING,
                        name);

  // Map the window to the display.
  connection_.MapWindow({window_});
  connection_.Flush();

  // Wait for map event.
  while (connection_.Ready()) {
    auto event = connection_.WaitForNextEvent();
    auto* map = event.As<x11::MapNotifyEvent>();
    if (map && map->event == window_)
      break;
  }

  size_ = {width, height};
  presenter_ = std::make_unique<ui::X11SoftwareBitmapPresenter>(
      &connection_, static_cast<gfx::AcceleratedWidget>(window_), false);
  presenter_->Resize(size_);
  connection_.Flush();
}

void LinuxOutputWindow::RenderFrame(const media::VideoFrame& video_frame) {
  const gfx::Size damage_size(
      std::min(video_frame.visible_rect().width(), size_.width()),
      std::min(video_frame.visible_rect().height(), size_.height()));

  if (damage_size.IsEmpty())
    return;

  PaintCanvasVideoRenderer renderer;
  cc::SkiaPaintCanvas canvas(presenter_->GetSkCanvas());
  renderer.Copy(const_cast<media::VideoFrame*>(&video_frame), &canvas, nullptr);
  presenter_->EndPaint(gfx::Rect(damage_size));

  // Very important for the image to update properly!
  connection_.Flush();
}

}  // namespace test
}  // namespace cast
}  // namespace media
