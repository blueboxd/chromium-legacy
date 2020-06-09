// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_JS_TO_JAVA_MESSAGING_H_
#define COMPONENTS_JS_INJECTION_BROWSER_JS_TO_JAVA_MESSAGING_H_

#include <vector>

#include "base/check.h"
#include "base/strings/string16.h"
#include "components/js_injection/common/aw_origin_matcher.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace content {
class RenderFrameHost;
}

namespace js_injection {

class WebMessageHost;
class WebMessageHostFactory;

// Implementation of mojo::JsToJavaMessaging interface. Receives PostMessage()
// call from renderer JsBinding.
class JsToJavaMessaging : public mojom::JsToJavaMessaging {
 public:
  JsToJavaMessaging(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<mojom::JsToJavaMessaging> receiver,
      WebMessageHostFactory* factory,
      const AwOriginMatcher& origin_matcher);
  ~JsToJavaMessaging() override;

  // mojom::JsToJavaMessaging implementation.
  void PostMessage(const base::string16& message,
                   std::vector<blink::MessagePortDescriptor> ports) override;
  void SetJavaToJsMessaging(
      mojo::PendingAssociatedRemote<mojom::JavaToJsMessaging>
          java_to_js_messaging) override;

 private:
  class ReplyProxyImpl;

  content::RenderFrameHost* render_frame_host_;
  std::unique_ptr<ReplyProxyImpl> reply_proxy_;
  WebMessageHostFactory* connection_factory_;
  AwOriginMatcher origin_matcher_;
  mojo::AssociatedReceiver<mojom::JsToJavaMessaging> receiver_{this};
  std::unique_ptr<WebMessageHost> host_;
#if DCHECK_IS_ON()
  std::string origin_string_;
  bool is_main_frame_;
#endif

  DISALLOW_COPY_AND_ASSIGN(JsToJavaMessaging);
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_JS_TO_JAVA_MESSAGING_H_
