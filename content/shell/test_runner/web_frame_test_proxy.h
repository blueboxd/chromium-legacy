// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

namespace content {
class RenderViewImpl;
}  // namespace content

namespace test_runner {

// WebFrameTestProxy is used during running web tests instead of a
// RenderFrameImpl to inject test-only behaviour by overriding methods in the
// base class.
class TEST_RUNNER_EXPORT WebFrameTestProxy : public content::RenderFrameImpl {
 public:
  template <typename... Args>
  explicit WebFrameTestProxy(Args&&... args)
      : RenderFrameImpl(std::forward<Args>(args)...) {}
  ~WebFrameTestProxy() override;

  void Initialize(content::RenderViewImpl* render_view_for_frame);

  // Returns a frame name that can be used in the output of web tests
  // (the name is derived from the frame's unique name).
  std::string GetFrameNameForWebTests();

  // RenderFrameImpl overrides.
  void UpdateAllLifecyclePhasesAndCompositeForTesting() override;

  // WebLocalFrameClient implementation.
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidChangeSelection(bool is_selection_empty) override;
  void DidChangeContents() override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void ShowContextMenu(
      const blink::WebContextMenuData& context_menu_data) override;
  void DidDispatchPingLoader(const blink::WebURL& url) override;
  void WillSendRequest(blink::WebURLRequest& request) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void PostAccessibilityEvent(const blink::WebAXObject& object,
                              ax::mojom::Event event,
                              ax::mojom::EventFrom event_from) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& object,
                            bool subtree) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback completion_callback) override;
  void DidClearWindowObject() override;

 private:
  std::unique_ptr<WebFrameTestClient> test_client_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
