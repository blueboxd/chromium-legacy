// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/mojo_data_pump.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace media_router {

MojoDataPump::MojoDataPump(mojo::ScopedDataPipeConsumerHandle receive_stream,
                           mojo::ScopedDataPipeProducerHandle send_stream)
    : receive_stream_(std::move(receive_stream)),
      receive_stream_watcher_(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_stream_(std::move(send_stream)),
      send_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  DCHECK(receive_stream_.is_valid());
  DCHECK(send_stream_.is_valid());
  receive_stream_watcher_.Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MojoDataPump::ReceiveMore, base::Unretained(this)));
  send_stream_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MojoDataPump::SendMore, base::Unretained(this)));
}

MojoDataPump::~MojoDataPump() {}

void MojoDataPump::Read(scoped_refptr<net::IOBuffer> io_buffer,
                        int io_buffer_size,
                        net::CompletionOnceCallback callback) {
  DCHECK(callback);
  DCHECK(!read_callback_);

  if (io_buffer_size <= 0) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  pending_read_buffer_ = std::move(io_buffer);
  pending_read_buffer_size_ = io_buffer_size;
  read_callback_ = std::move(callback);
  receive_stream_watcher_.ArmOrNotify();
}

void MojoDataPump::Write(scoped_refptr<net::IOBuffer> io_buffer,
                         int io_buffer_size,
                         net::CompletionOnceCallback callback) {
  DCHECK(callback);
  DCHECK(!write_callback_);

  write_callback_ = std::move(callback);
  pending_write_buffer_ = std::move(io_buffer);
  pending_write_buffer_size_ = io_buffer_size;
  send_stream_watcher_.ArmOrNotify();
}

bool MojoDataPump::HasPendingRead() const {
  return !read_callback_.is_null();
}

bool MojoDataPump::HasPendingWrite() const {
  return !write_callback_.is_null();
}

void MojoDataPump::ReceiveMore(MojoResult result,
                               const mojo::HandleSignalsState& state) {
  DCHECK(read_callback_);
  DCHECK_NE(0u, pending_read_buffer_size_);

  uint32_t num_bytes = pending_read_buffer_size_;
  if (result == MOJO_RESULT_OK) {
    result = receive_stream_->ReadData(pending_read_buffer_->data(), &num_bytes,
                                       MOJO_READ_DATA_FLAG_NONE);
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_stream_watcher_.ArmOrNotify();
    return;
  }

  pending_read_buffer_ = nullptr;
  pending_read_buffer_size_ = 0;
  if (result != MOJO_RESULT_OK) {
    std::move(read_callback_).Run(net::ERR_FAILED);
    return;
  }
  std::move(read_callback_).Run(num_bytes);
}

void MojoDataPump::SendMore(MojoResult result,
                            const mojo::HandleSignalsState& state) {
  DCHECK(write_callback_);

  uint32_t num_bytes = pending_write_buffer_size_;
  if (result == MOJO_RESULT_OK) {
    result = send_stream_->WriteData(pending_write_buffer_->data(), &num_bytes,
                                     MOJO_WRITE_DATA_FLAG_NONE);
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_stream_watcher_.ArmOrNotify();
    return;
  }

  pending_write_buffer_ = nullptr;
  pending_write_buffer_size_ = 0;
  if (result != MOJO_RESULT_OK) {
    std::move(write_callback_).Run(net::ERR_FAILED);
    return;
  }
  std::move(write_callback_).Run(num_bytes);
}

}  // namespace media_router
