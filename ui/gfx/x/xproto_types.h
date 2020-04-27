// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_TYPES_H_
#define UI_GFX_X_XPROTO_TYPES_H_

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <cstdint>
#include <memory>

#include "base/memory/free_deleter.h"
#include "base/optional.h"

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

template <typename Reply>
class Future {
 public:
  ~Future() {
    if (sequence_)
      xcb_discard_reply(conn_, *sequence_);
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

  Response<Reply> Sync() {
    if (!sequence_)
      return {{}, {}};

    Error* raw_error = nullptr;
    uint8_t* raw_reply = reinterpret_cast<uint8_t*>(
        xcb_wait_for_reply(conn_, *sequence_, &raw_error));
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

 private:
  template <typename R>
  friend Future<R> SendRequest(xcb_connection_t*, std::vector<uint8_t>*);

  Future(xcb_connection_t* conn, base::Optional<unsigned int> sequence)
      : conn_(conn), sequence_(sequence) {}

  xcb_connection_t* conn_;
  base::Optional<unsigned int> sequence_;
};

template <>
inline Response<void> Future<void>::Sync() {
  if (!sequence_)
    return Response<void>(nullptr);

  Error* raw_error = xcb_request_check(conn_, {*sequence_});
  std::unique_ptr<Error, base::FreeDeleter> error;
  if (raw_error)
    error.reset(raw_error);

  return Response<void>{std::move(error)};
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_TYPES_H_
