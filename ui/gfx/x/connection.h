// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include "base/component_export.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto {
 public:
  // Gets or creates the singeton connection.
  static Connection* Get();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

 private:
  explicit Connection(XDisplay* display);
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
