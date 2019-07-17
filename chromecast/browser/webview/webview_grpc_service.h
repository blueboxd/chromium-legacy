// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/server.h"

namespace chromecast {

// This is a service that provides a GRPC interface to create and control
// webviews. See the proto file for commands.
class WebviewAsyncService : public base::PlatformThread::Delegate {
 public:
  explicit WebviewAsyncService(
      scoped_refptr<base::SingleThreadTaskRunner> webview_task_runner);
  ~WebviewAsyncService() override;

  // Start the server listening on an address, eg "localhost:12345".
  void StartWithSocket(const base::FilePath& socket_path);

 private:
  void ThreadMain() override;

  base::PlatformThreadHandle rpc_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> webview_task_runner_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  webview::WebviewService::AsyncService service_;
  std::unique_ptr<grpc::Server> server_;

  DISALLOW_COPY_AND_ASSIGN(WebviewAsyncService);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_
