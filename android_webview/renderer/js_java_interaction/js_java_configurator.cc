// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/js_java_interaction/js_java_configurator.h"

#include "android_webview/renderer/js_java_interaction/js_binding.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

JsJavaConfigurator::JsJavaConfigurator(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&JsJavaConfigurator::BindPendingReceiver,
                          base::Unretained(this)));
}

JsJavaConfigurator::~JsJavaConfigurator() = default;

void JsJavaConfigurator::SetJsApiService(
    bool need_to_inject_js_object,
    const std::string& js_object_name,
    const net::ProxyBypassRules& allowed_origin_rules) {
  need_to_inject_js_object_ = need_to_inject_js_object;
  js_object_name_ = js_object_name;
  js_object_allowed_origin_rules_ = allowed_origin_rules;
}

void JsJavaConfigurator::DidClearWindowObject() {
  if (!need_to_inject_js_object_ || !IsOriginMatch())
    return;

  if (inside_did_clear_window_object_)
    return;

  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);
  js_binding_ = JsBinding::Install(render_frame(), js_object_name_);
}

void JsJavaConfigurator::OnDestruct() {
  delete this;
}

inline bool JsJavaConfigurator::IsOriginMatch() {
  url::Origin frame_origin =
      url::Origin(render_frame()->GetWebFrame()->GetSecurityOrigin());
  return js_object_allowed_origin_rules_.Matches(frame_origin.GetURL());
}

void JsJavaConfigurator::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::JsJavaConfigurator>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver),
                 render_frame()->GetTaskRunner(blink::TaskType::kInternalIPC));
}

}  // namespace android_webview
