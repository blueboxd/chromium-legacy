// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_DATA_PUMP_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_DATA_PUMP_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace media_router {

class NetworkDataPump {
 public:
  virtual ~NetworkDataPump() = default;

  virtual void Read(scoped_refptr<net::IOBuffer> io_buffer,
                    int io_buffer_size,
                    net::CompletionOnceCallback callback) = 0;
  virtual void Write(scoped_refptr<net::IOBuffer> io_buffer,
                     int io_buffer_size,
                     net::CompletionOnceCallback callback) = 0;

  virtual bool HasPendingRead() const = 0;
  virtual bool HasPendingWrite() const = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_DATA_PUMP_H_
