// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_
#define ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"

namespace content {
class RenderFrame;
}

namespace android_webview {

class JsBinding;

class JsJavaConfigurator : public mojom::JsJavaConfigurator,
                           public content::RenderFrameObserver {
 public:
  explicit JsJavaConfigurator(content::RenderFrame* render_frame);
  ~JsJavaConfigurator() override;

  // mojom::Configurator implementation
  void SetJsApiService(
      bool inject_js_object,
      const std::string& js_object_name,
      const net::ProxyBypassRules& allowed_origin_rules) override;

  // RenderFrameObserver implementation
  void DidClearWindowObject() override;
  void OnDestruct() override;

 private:
  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::JsJavaConfigurator>
          pending_receiver);

  bool IsOriginMatch();

  bool need_to_inject_js_object_ = false;
  std::string js_object_name_;
  // We use ProxyBypassRules because it has the functionality that suitable
  // here, but it is not for proxy bypass.
  net::ProxyBypassRules js_object_allowed_origin_rules_;

  // In some cases DidClearWindowObject will be called twice in a row, we need
  // to prevent doing multiple injection in that case.
  bool inside_did_clear_window_object_ = false;

  std::unique_ptr<JsBinding> js_binding_;

  // Associated with legacy IPC channel.
  mojo::AssociatedReceiver<mojom::JsJavaConfigurator> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsJavaConfigurator);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_
