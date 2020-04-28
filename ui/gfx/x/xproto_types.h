// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_TYPES_H_
#define UI_GFX_X_XPROTO_TYPES_H_

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/free_deleter.h"
#include "base/optional.h"
#include "ui/gfx/x/request_queue.h"
#include "ui/gfx/x/xproto_util.h"

typedef struct _XDisplay XDisplay;

namespace x11 {

namespace detail {

template <typename Reply>
std::unique_ptr<Reply> ReadReply(const uint8_t* buffer);

}

using Error = xcb_generic_error_t;

template <class Reply>
class Future;

template <typename Reply>
struct Response {
  operator bool() const { return reply.get(); }
  const Reply* operator->() const { return reply.get(); }

  std::unique_ptr<Reply> reply;
  std::unique_ptr<Error, base::FreeDeleter> error;

 private:
  friend class Future<Reply>;

  Response(std::unique_ptr<Reply> reply,
           std::unique_ptr<Error, base::FreeDeleter> error)
      : reply(std::move(reply)), error(std::move(error)) {}
};

template <>
struct Response<void> {
  std::unique_ptr<Error, base::FreeDeleter> error;

 private:
  friend class Future<void>;

  explicit Response(std::unique_ptr<Error, base::FreeDeleter> error)
      : error(std::move(error)) {}
};

// An x11::Future wraps an asynchronous response from the X11 server.  The
// response may be waited-for with Sync(), or asynchronously handled by
// installing a response handler using OnResponse().
template <typename Reply>
class Future {
 public:
  using Callback = base::OnceCallback<void(Response<Reply> response)>;
  using RQ = RequestQueue;

  // If a user-defined response-handler is not installed before this object goes
  // out of scope, a default response handler will be installed.  The default
  // handler throws away the reply and prints the error if there is one.
  ~Future() {
    if (!sequence_)
      return;

    EnqueueRequest(base::BindOnce(
        [](XDisplay* display, RQ::Reply reply, RQ::Error error) {
          if (!error)
            return;

          x11::LogErrorEventDescription(XErrorEvent({
              .type = error->response_type,
              .display = display,
              .resourceid = error->resource_id,
              .serial = error->full_sequence,
              .error_code = error->error_code,
              .request_code = error->major_code,
              .minor_code = error->minor_code,
          }));
        },
        display_));
  }

  Future(const Future&) = delete;
  Future& operator=(const Future&) = delete;

  Future(Future&& future) : sequence_(future.sequence_) {
    future.sequence_ = base::nullopt;
  }
  Future& operator=(Future&& future) {
    sequence_ = future.sequence_;
    future.sequence_ = base::nullopt;
  }

  xcb_connection_t* connection() { return XGetXCBConnection(display_); }

  // Blocks until we receive the response from the server. Returns the response.
  Response<Reply> Sync() {
    if (!sequence_)
      return {{}, {}};

    Error* raw_error = nullptr;
    uint8_t* raw_reply = reinterpret_cast<uint8_t*>(
        xcb_wait_for_reply(connection(), *sequence_, &raw_error));
    sequence_ = base::nullopt;

    std::unique_ptr<Reply> reply;
    if (raw_reply) {
      reply = detail::ReadReply<Reply>(raw_reply);
      free(raw_reply);
    }

    std::unique_ptr<Error, base::FreeDeleter> error;
    if (raw_error)
      error.reset(raw_error);

    return {std::move(reply), std::move(error)};
  }

  // Installs |callback| to be run when the response is received.
  void OnResponse(Callback callback) {
    if (!sequence_)
      return;

    // This intermediate callback handles the conversion from |raw_reply| to a
    // real Reply object before feeding the result to |callback|.  This means
    // |callback| must be bound as the first argument of the intermediate
    // function.
    auto wrapper = [](Callback callback, RQ::Reply raw_reply, RQ::Error error) {
      std::unique_ptr<Reply> reply =
          raw_reply ? detail::ReadReply<Reply>(raw_reply.get()) : nullptr;
      std::move(callback).Run({std::move(reply), std::move(error)});
    };
    EnqueueRequest(base::BindOnce(wrapper, std::move(callback)));

    sequence_ = base::nullopt;
  }

 private:
  template <typename R>
  friend Future<R> SendRequest(XDisplay*, std::vector<uint8_t>*);

  Future(XDisplay* display, base::Optional<unsigned int> sequence)
      : display_(display), sequence_(sequence) {}

  void EnqueueRequest(RQ::ResponseCallback callback) {
    RQ::GetInstance()->AddRequest(std::is_void<Reply>::value, *sequence_,
                                  std::move(callback));
  }

  XDisplay* display_;
  base::Optional<unsigned int> sequence_;
};

// Sync() specialization for requests that don't generate replies.  The returned
// response will only contain an error if there was one.
template <>
inline Response<void> Future<void>::Sync() {
  if (!sequence_)
    return Response<void>(nullptr);

  Error* raw_error = xcb_request_check(connection(), {*sequence_});
  std::unique_ptr<Error, base::FreeDeleter> error;
  if (raw_error)
    error.reset(raw_error);

  return Response<void>{std::move(error)};
}

// OnResponse() specialization for requests that don't generate replies.  The
// response argument to |callback| will only contain an error if there was one.
template <>
inline void Future<void>::OnResponse(Callback callback) {
  if (!sequence_)
    return;

  // See Future<Reply>::OnResponse() for an explanation of why
  // this wrapper is necessary.
  auto wrapper = [](Callback callback, RQ::Reply reply, RQ::Error error) {
    DCHECK(!reply);
    std::move(callback).Run(Response<void>{std::move(error)});
  };
  EnqueueRequest(base::BindOnce(wrapper, std::move(callback)));

  sequence_ = base::nullopt;
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_TYPES_H_
