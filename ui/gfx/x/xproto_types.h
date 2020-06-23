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
#include "ui/gfx/x/xproto_util.h"

typedef struct _XDisplay XDisplay;

namespace x11 {

class Connection;

constexpr uint8_t kSendEventMask = 0x80;

namespace detail {

template <typename Reply>
std::unique_ptr<Reply> ReadReply(const uint8_t* buffer);

}  // namespace detail

using Error = xcb_generic_error_t;

template <class Reply>
class Future;

template <typename T>
T Read(const uint8_t* buf);

template <typename T>
std::vector<uint8_t> Write(const T& t);

template <typename T>
void ReadEvent(T* event, const uint8_t* buf);

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

class COMPONENT_EXPORT(X11) FutureBase {
 public:
  using RawReply = std::unique_ptr<uint8_t, base::FreeDeleter>;
  using RawError = std::unique_ptr<xcb_generic_error_t, base::FreeDeleter>;
  using ResponseCallback =
      base::OnceCallback<void(RawReply reply, RawError error)>;

  FutureBase(const FutureBase&) = delete;
  FutureBase& operator=(const FutureBase&) = delete;

 protected:
  FutureBase(Connection* connection, base::Optional<unsigned int> sequence);
  ~FutureBase();

  FutureBase(FutureBase&& future);
  FutureBase& operator=(FutureBase&& future);

  void SyncImpl(Error** raw_error, uint8_t** raw_reply);
  void SyncImpl(Error** raw_error);

  void OnResponseImpl(ResponseCallback callback);

 private:
  Connection* connection_;
  base::Optional<unsigned int> sequence_;
};

// An x11::Future wraps an asynchronous response from the X11 server.  The
// response may be waited-for with Sync(), or asynchronously handled by
// installing a response handler using OnResponse().
template <typename Reply>
class Future : public FutureBase {
 public:
  using Callback = base::OnceCallback<void(Response<Reply> response)>;

  Future() : FutureBase(nullptr, base::nullopt) {}

  // Blocks until we receive the response from the server. Returns the response.
  Response<Reply> Sync() {
    Error* raw_error = nullptr;
    uint8_t* raw_reply = nullptr;
    SyncImpl(&raw_error, &raw_reply);

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
    // This intermediate callback handles the conversion from |raw_reply| to a
    // real Reply object before feeding the result to |callback|.  This means
    // |callback| must be bound as the first argument of the intermediate
    // function.
    auto wrapper = [](Callback callback, RawReply raw_reply, RawError error) {
      std::unique_ptr<Reply> reply =
          raw_reply ? detail::ReadReply<Reply>(raw_reply.get()) : nullptr;
      std::move(callback).Run({std::move(reply), std::move(error)});
    };
    OnResponseImpl(base::BindOnce(wrapper, std::move(callback)));
  }

  void IgnoreError() {
    OnResponse(base::BindOnce([](Response<Reply>) {}));
  }

 private:
  template <typename R>
  friend Future<R> SendRequest(Connection*, std::vector<uint8_t>*);

  Future(Connection* connection, base::Optional<unsigned int> sequence)
      : FutureBase(connection, sequence) {}
};

// Sync() specialization for requests that don't generate replies.  The returned
// response will only contain an error if there was one.
template <>
inline Response<void> Future<void>::Sync() {
  Error* raw_error = nullptr;
  SyncImpl(&raw_error);

  std::unique_ptr<Error, base::FreeDeleter> error;
  if (raw_error)
    error.reset(raw_error);

  return Response<void>{std::move(error)};
}

// OnResponse() specialization for requests that don't generate replies.  The
// response argument to |callback| will only contain an error if there was one.
template <>
inline void Future<void>::OnResponse(Callback callback) {
  // See Future<Reply>::OnResponse() for an explanation of why
  // this wrapper is necessary.
  auto wrapper = [](Callback callback, RawReply reply, RawError error) {
    DCHECK(!reply);
    std::move(callback).Run(Response<void>{std::move(error)});
  };
  OnResponseImpl(base::BindOnce(wrapper, std::move(callback)));
}

template <>
inline void Future<void>::IgnoreError() {
  OnResponse(base::BindOnce([](Response<void>) {}));
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_TYPES_H_
