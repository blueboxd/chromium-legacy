// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/mock_screen_orientation_client.h"
#include "content/shell/renderer/web_test/test_interfaces.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_frame_test_proxy.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebViewTestProxy::WebViewTestProxy(CompositorDependencies* compositor_deps,
                                   const mojom::CreateViewParams& params,
                                   TestInterfaces* interfaces)
    : RenderViewImpl(compositor_deps, params), test_interfaces_(interfaces) {
  test_interfaces_->WindowOpened(this);
}

blink::WebView* WebViewTestProxy::CreateView(
    blink::WebLocalFrame* creator,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::FeaturePolicy::FeatureState& opener_feature_state,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  if (GetTestRunner()->ShouldDumpNavigationPolicy()) {
    blink_test_runner()->PrintMessage(
        "Default policy for createView for '" +
        web_test_string_util::URLDescription(request.Url()) + "' is '" +
        web_test_string_util::WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!GetTestRunner()->CanOpenWindows())
    return nullptr;

  if (GetTestRunner()->ShouldDumpCreateView()) {
    blink_test_runner()->PrintMessage(
        std::string("createView(") +
        web_test_string_util::URLDescription(request.Url()) + ")\n");
  }
  return RenderViewImpl::CreateView(creator, request, features, frame_name,
                                    policy, sandbox_flags, opener_feature_state,
                                    session_storage_namespace_id);
}

void WebViewTestProxy::PrintPage(blink::WebLocalFrame* frame) {
  // This is using the main frame for the size, but maybe it should be using the
  // frame's size.
  blink::WebSize page_size_in_pixels =
      GetMainRenderFrame()->GetLocalRootRenderWidget()->GetWebWidget()->Size();
  if (page_size_in_pixels.IsEmpty())
    return;
  blink::WebPrintParams print_params(page_size_in_pixels);
  frame->PrintBegin(print_params);
  frame->PrintEnd();
}

blink::WebString WebViewTestProxy::AcceptLanguages() {
  return blink::WebString::FromUTF8(GetTestRunner()->GetAcceptLanguages());
}

void WebViewTestProxy::Reset() {
  accessibility_controller_.Reset();
  // |text_input_controller_| doesn't have any state to reset.

  view_test_runner_.Reset();

  // Resets things on the WebView that TestRunnerBindings can modify.
  GetWebView()->SetTabKeyCyclesThroughElements(true);

  for (blink::WebFrame* frame = GetWebView()->MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame()) {
      RenderFrame* render_frame =
          RenderFrame::FromWebFrame(frame->ToWebLocalFrame());
      auto* frame_proxy = static_cast<WebFrameTestProxy*>(render_frame);
      frame_proxy->Reset();
    }
  }
}

void WebViewTestProxy::Install(blink::WebLocalFrame* frame) {
  accessibility_controller_.Install(frame);
  text_input_controller_.Install(frame);
}

WebViewTestProxy::~WebViewTestProxy() {
  test_interfaces_->WindowClosed(this);
}

TestRunner* WebViewTestProxy::GetTestRunner() {
  return test_interfaces_->GetTestRunner();
}

}  // namespace content
