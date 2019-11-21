// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_MOJO_DATA_PUMP_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_MOJO_DATA_PUMP_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/router/providers/openscreen/platform/network_data_pump.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace media_router {

// Helper class to read from a mojo consumer handle and write to mojo producer
// handle.
// NOTE: this class is copied from the cast channel implementation, with
// changes made to meet our needs.
class MojoDataPump : public NetworkDataPump {
 public:
  MojoDataPump(mojo::ScopedDataPipeConsumerHandle receive_stream,
               mojo::ScopedDataPipeProducerHandle send_stream);
  MojoDataPump(MojoDataPump&&) = delete;
  MojoDataPump(const MojoDataPump&) = delete;
  MojoDataPump& operator=(MojoDataPump&) = delete;
  MojoDataPump& operator=(const MojoDataPump&) = delete;
  ~MojoDataPump() override;

  void Read(scoped_refptr<net::IOBuffer> io_buffer,
            int io_buffer_size,
            net::CompletionOnceCallback callback) override;
  void Write(scoped_refptr<net::IOBuffer> io_buffer,
             int io_buffer_size,
             net::CompletionOnceCallback callback) override;

  bool HasPendingRead() const override;
  bool HasPendingWrite() const override;

 private:
  void OnReadComplete(int result);
  void StartWatching();
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);
  void SendMore(MojoResult result, const mojo::HandleSignalsState& state);

  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::SimpleWatcher send_stream_watcher_;

  net::CompletionOnceCallback read_callback_;
  net::CompletionOnceCallback write_callback_;
  scoped_refptr<net::IOBuffer> pending_read_buffer_;
  scoped_refptr<net::IOBuffer> pending_write_buffer_;
  size_t pending_write_buffer_size_ = 0;
  size_t pending_read_buffer_size_ = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_MOJO_DATA_PUMP_H_
