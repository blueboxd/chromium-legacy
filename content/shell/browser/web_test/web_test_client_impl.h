// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_

#include <vector>

#include "content/shell/common/web_test/web_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

// WebTestClientImpl is an implementation of WebTestClient mojo interface that
// handles the communication from the renderer process to the browser process
// using the legacy IPC. This class is bound to a RenderProcessHost when it's
// initialized and is managed by the registry of the RenderProcessHost.
class WebTestClientImpl : public mojom::WebTestClient {
 public:
  static void Create(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::WebTestClient> receiver);

  explicit WebTestClientImpl(int render_process_id);
  ~WebTestClientImpl() override;

  WebTestClientImpl(const WebTestClientImpl&) = delete;
  WebTestClientImpl& operator=(const WebTestClientImpl&) = delete;

 private:
  // WebTestClient implementation.
  void WebTestRuntimeFlagsChanged(
      base::Value changed_web_test_runtime_flags) override;
  void RegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& file_paths,
      RegisterIsolatedFileSystemCallback callback) override;

  const int render_process_id_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
