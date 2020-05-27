// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

#include <algorithm>

#include "base/command_line.h"
#include "ui/gfx/x/x11_switches.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

namespace {

XDisplay* OpenNewXDisplay() {
  if (!XInitThreads())
    return nullptr;
  std::string display_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kX11Display);
  return XOpenDisplay(display_str.empty() ? nullptr : display_str.c_str());
}

}  // namespace

Connection* Connection::Get() {
  static Connection* instance = new Connection(OpenNewXDisplay());
  return instance;
}

Connection::Connection(XDisplay* display)
    : XProto(display), ExtensionManager(this) {
  if (!display)
    return;

  setup_ = std::make_unique<x11::XProto::Setup>(x11::Read<x11::XProto::Setup>(
      reinterpret_cast<const uint8_t*>(xcb_get_setup(XcbConnection()))));
  default_screen_ = &setup_->roots[DefaultScreen(display)];
  default_root_depth_ =
      &*std::find_if(default_screen_->allowed_depths.begin(),
                     default_screen_->allowed_depths.end(),
                     [&](const x11::XProto::Depth& depth) {
                       return depth.depth == default_screen_->root_depth;
                     });
  defualt_root_visual_ = &*std::find_if(
      default_root_depth_->visuals.begin(), default_root_depth_->visuals.end(),
      [&](const x11::XProto::VisualType visual) {
        return visual.visual_id == default_screen_->root_visual;
      });
}

Connection::~Connection() = default;

xcb_connection_t* Connection::XcbConnection() {
  if (!display())
    return nullptr;
  return XGetXCBConnection(display());
}

}  // namespace x11
