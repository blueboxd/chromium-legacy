// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_REQUEST_QUEUE_H_
#define UI_GFX_X_REQUEST_QUEUE_H_

#include <xcb/xcb.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/free_deleter.h"

namespace ui {
class X11EventSource;
}

namespace x11 {

// This interface allows //ui/gfx/x to call into //ui/events/platform/x11 which
// is at a higher layer.  It should not be used by client code.
class COMPONENT_EXPORT(X11) RequestQueue {
 private:
  friend class ui::X11EventSource;
  template <typename T>
  friend class Future;

  using Reply = std::unique_ptr<uint8_t, base::FreeDeleter>;
  using Error = std::unique_ptr<xcb_generic_error_t, base::FreeDeleter>;
  using ResponseCallback = base::OnceCallback<void(Reply reply, Error error)>;

  RequestQueue();
  virtual ~RequestQueue();

  // Adds a request to the queue.  |is_void| indicates if a reply is generated
  // for this request.  |sequence| is the ID of the request.  |callback| will
  // be called upon request completion (or failure).
  virtual void AddRequest(bool is_void,
                          unsigned int sequence,
                          ResponseCallback callback) = 0;

  static RequestQueue* GetInstance();

  static RequestQueue* instance_;
};

}  // namespace x11

#endif  //  UI_GFX_X_REQUEST_QUEUE_H_
