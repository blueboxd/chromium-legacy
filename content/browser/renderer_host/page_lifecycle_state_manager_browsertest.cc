// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class PageLifecycleStateManagerBrowserTest : public ContentBrowserTest {
 public:
  ~PageLifecycleStateManagerBrowserTest() override = default;

 protected:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void StartRecordingEvents(RenderFrameHostImpl* rfh) {
    EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'freeze',
        'resume',
      ];
      for (event_name of event_list) {
        let result = event_name;
        document.addEventListener(event_name, event => {
          window.testObservedEvents.push('document.' + result);
        });
      }
    )"));
  }

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::ListValue list,
                      base::Location location = base::Location::Current()) {
    EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
        << location.ToString();
  }

  RenderViewHostImpl* render_view_host() {
    return static_cast<RenderViewHostImpl*>(
        shell()->web_contents()->GetRenderViewHost());
  }

  RenderFrameHostImpl* current_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetFrameTree()
        ->root()
        ->current_frame_host();
  }
};

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest, SetFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderViewHostImpl* rvh = render_view_host();
  RenderFrameHostImpl* rfh = current_frame_host();
  StartRecordingEvents(rfh);
  // TODO(yuzus):Use PageLifecycleStateManager for visibility change.
  shell()->web_contents()->WasHidden();

  rvh->SetIsFrozen(/*frozen*/ true);
  rvh->SetIsFrozen(/*frozen*/ false);
  MatchEventList(rfh, ListValueOf("document.freeze", "document.resume"));
}

}  // namespace content
