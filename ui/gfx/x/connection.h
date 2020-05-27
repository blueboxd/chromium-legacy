// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include "base/component_export.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

using Atom = XProto::Atom;
using Window = XProto::Window;

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto,
                                         public ExtensionManager {
 public:
  // Gets or creates the singeton connection.
  static Connection* Get();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

  xcb_connection_t* XcbConnection();

  const x11::XProto::Setup* setup() const { return setup_.get(); }
  const x11::XProto::Screen* default_screen() const { return default_screen_; }
  const x11::XProto::Depth* default_root_depth() const {
    return default_root_depth_;
  }
  const x11::XProto::VisualType* default_root_visual() const {
    return defualt_root_visual_;
  }

 private:
  explicit Connection(XDisplay* display);
  ~Connection();

  std::unique_ptr<x11::XProto::Setup> setup_;
  const x11::XProto::Screen* default_screen_ = nullptr;
  const x11::XProto::Depth* default_root_depth_ = nullptr;
  const x11::XProto::VisualType* defualt_root_visual_ = nullptr;
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
