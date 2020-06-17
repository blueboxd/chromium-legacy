// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_types.h"

#include "ui/gfx/x/connection.h"

namespace x11 {

FutureBase::FutureBase(Connection* connection,
                       base::Optional<unsigned int> sequence)
    : connection_(connection), sequence_(sequence) {}

// If a user-defined response-handler is not installed before this object goes
// out of scope, a default response handler will be installed.  The default
// handler throws away the reply and prints the error if there is one.
FutureBase::~FutureBase() {
  if (!sequence_)
    return;

  OnResponseImpl(base::BindOnce(
      [](Connection* connection, RawReply reply, RawError error) {
        if (!error)
          return;

        x11::LogErrorEventDescription(XErrorEvent({
            .type = error->response_type,
            .display = connection->display(),
            .resourceid = error->resource_id,
            .serial = error->full_sequence,
            .error_code = error->error_code,
            .request_code = error->major_code,
            .minor_code = error->minor_code,
        }));
      },
      connection_));
}

FutureBase::FutureBase(FutureBase&& future)
    : connection_(future.connection_), sequence_(future.sequence_) {
  future.connection_ = nullptr;
  future.sequence_ = base::nullopt;
}

FutureBase& FutureBase::operator=(FutureBase&& future) {
  connection_ = future.connection_;
  sequence_ = future.sequence_;
  future.connection_ = nullptr;
  future.sequence_ = base::nullopt;
  return *this;
}

void FutureBase::SyncImpl(Error** raw_error, uint8_t** raw_reply) {
  if (!sequence_)
    return;
  *raw_reply = reinterpret_cast<uint8_t*>(
      xcb_wait_for_reply(connection_->XcbConnection(), *sequence_, raw_error));
  sequence_ = base::nullopt;
}

void FutureBase::SyncImpl(Error** raw_error) {
  if (!sequence_)
    return;
  *raw_error = xcb_request_check(connection_->XcbConnection(), {*sequence_});
  sequence_ = base::nullopt;
}

void FutureBase::OnResponseImpl(ResponseCallback callback) {
  if (!sequence_)
    return;
  connection_->AddRequest(*sequence_, std::move(callback));
  sequence_ = base::nullopt;
}

}  // namespace x11
