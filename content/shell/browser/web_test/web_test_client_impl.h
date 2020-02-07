// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_

#include "base/macros.h"
#include "content/shell/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// WebTestClientImpl is an implementation of WebTestClient mojo interface that
// handles the communication from the renderer process to the browser process
// using the legacy IPC. This class is bound to a RenderProcessHost when it's
// initialized and is managed by the registry of the RenderProcessHost.
class WebTestClientImpl : public mojom::WebTestClient {
 public:
  WebTestClientImpl() = default;
  ~WebTestClientImpl() override = default;

  WebTestClientImpl(const WebTestClientImpl&) = delete;
  WebTestClientImpl& operator=(const WebTestClientImpl&) = delete;

  static void Create(mojo::PendingReceiver<mojom::WebTestClient> receiver);

 private:
  // WebTestClient implementation.
  void InspectSecondaryWindow() override;
  void TestFinishedInSecondaryRenderer() override;
  void SimulateWebNotificationClose(const std::string& title,
                                    bool by_user) override;
  void SimulateWebContentIndexDelete(const std::string& id) override;
  void BlockThirdPartyCookies(bool block) override;
  void ResetPermissions() override;
  void SetPermission(const std::string& name,
                     blink::mojom::PermissionStatus status,
                     const GURL& origin,
                     const GURL& embedding_origin) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
