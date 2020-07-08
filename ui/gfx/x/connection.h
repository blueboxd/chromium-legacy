// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include <list>
#include <queue>

#include "base/component_export.h"
#include "ui/gfx/x/event.h"
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
    virtual void DispatchXEvent(x11::Event* event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Gets or creates the singleton connection.
  static Connection* Get();

  explicit Connection();
  ~Connection();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

  XDisplay* display() const { return display_; }
  xcb_connection_t* XcbConnection();

  uint32_t extended_max_request_length() const {
    return extended_max_request_length_;
  }

  const Setup& setup() const { return setup_; }
  const Screen& default_screen() const { return *default_screen_; }
  x11::Window default_root() const { return default_screen().root; }
  const Depth& default_root_depth() const { return *default_root_depth_; }
  const VisualType& default_root_visual() const {
    return *default_root_visual_;
  }

  int DefaultScreenId() const;

  template <typename T>
  T GenerateId() {
    return static_cast<T>(xcb_generate_id(XcbConnection()));
  }

  // Is the connection up and error-free?
  bool Ready() const;

  // Write all requests to the socket.
  void Flush();

  // Flush and block until the server has responded to all requests.
  void Sync();

  // Read all responses from the socket without blocking.
  void ReadResponses();

  // Are there any events, errors, or replies already buffered?
  bool HasPendingResponses() const;

  // Dispatch any buffered events, errors, or replies.
  void Dispatch(Delegate* delegate);

  // Access the event buffer.  Clients can add, delete, or modify events.
  std::list<Event>& events() { return events_; }

 private:
  friend class FutureBase;

  struct Request {
    Request(unsigned int sequence, FutureBase::ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    const unsigned int sequence;
    FutureBase::ResponseCallback callback;
  };

  void AddRequest(unsigned int sequence, FutureBase::ResponseCallback callback);

  bool HasNextResponse() const;

  void PreDispatchEvent(const Event& event);

  int ScreenIndexFromRootWindow(x11::Window root) const;

  XDisplay* const display_;

  uint32_t extended_max_request_length_ = 0;

  Setup setup_;
  Screen* default_screen_ = nullptr;
  Depth* default_root_depth_ = nullptr;
  VisualType* default_root_visual_ = nullptr;

  std::list<Event> events_;

  std::queue<Request> requests_;
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
