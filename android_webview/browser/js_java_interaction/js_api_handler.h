// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_

#include <string>
#include <vector>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {
class RenderFrameHost;
}

namespace android_webview {

// Implementation of mojo::JsApiHandler interface. Receives PostMessage() call
// from renderer JsBinding.
class JsApiHandler : public mojom::JsApiHandler {
 public:
  explicit JsApiHandler(content::RenderFrameHost* rfh);
  ~JsApiHandler() override;

  // Binds mojo.
  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::JsApiHandler> pending_receiver);

  // mojom::JsApiHandler implementation.
  void PostMessage(const std::string& message,
                   std::vector<mojo::ScopedMessagePipeHandle> ports) override;

 private:
  content::RenderFrameHost* render_frame_host_;
  mojo::AssociatedReceiver<mojom::JsApiHandler> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsApiHandler);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_
