// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include <queue>

#include "base/component_export.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto,
                                         public ExtensionManager {
 public:
  class Delegate {
   public:
    virtual bool ShouldContinueStream() const = 0;
    virtual void DispatchXEvent(XEvent* event) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Gets or creates the singeton connection.
  static Connection* Get();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

  XDisplay* display() const { return display_; }
  xcb_connection_t* XcbConnection();

  uint32_t extended_max_request_length() const {
    return extended_max_request_length_;
  }

  const x11::Setup* setup() const { return setup_.get(); }
  const x11::Screen* default_screen() const { return default_screen_; }
  const x11::Depth* default_root_depth() const { return default_root_depth_; }
  const x11::VisualType* default_root_visual() const {
    return defualt_root_visual_;
  }

  void Dispatch(Delegate* delegate);

 private:
  friend class FutureBase;

  struct Request {
    Request(unsigned int sequence, FutureBase::ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    const unsigned int sequence;
    FutureBase::ResponseCallback callback;
  };

  explicit Connection(XDisplay* display);
  ~Connection();

  void AddRequest(unsigned int sequence, FutureBase::ResponseCallback callback);

  XDisplay* const display_;

  uint32_t extended_max_request_length_ = 0;

  std::unique_ptr<x11::Setup> setup_;
  const x11::Screen* default_screen_ = nullptr;
  const x11::Depth* default_root_depth_ = nullptr;
  const x11::VisualType* defualt_root_visual_ = nullptr;

  std::queue<Request> requests_;
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
