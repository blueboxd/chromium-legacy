// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest_mac.h"
#include "ui/base/cocoa/find_pasteboard.h"
#include "url/gurl.h"

namespace content {

using RenderFrameHostImplBrowserMacTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserMacTest,
                       CopyToFindPasteboard) {
  WebContents* web_contents = shell()->web_contents();

  GURL url("data:text/html,Hello world");
  ASSERT_TRUE(NavigateToURL(web_contents, url));

  FindPasteboard* pboard = [FindPasteboard sharedInstance];
  base::scoped_nsobject<NSString> original_pboard_text(
      [[pboard findText] copy]);

  [pboard setFindText:@"test"];
  EXPECT_NSEQ(@"test", [pboard findText]);

  auto* rfhi = static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  auto* input_handler = rfhi->GetFrameInputHandler();
  input_handler->SelectAll();
  input_handler->CopyToFindPboard();

  base::RunLoop loop;
  __block base::OnceClosure quit_closure = loop.QuitClosure();

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  id notification_handle =
      [center addObserverForName:kFindPasteboardChangedNotification
                          object:pboard
                           queue:nil
                      usingBlock:^(NSNotification*) {
                        std::move(quit_closure).Run();
                      }];
  loop.Run();

  [center removeObserver:notification_handle];

  EXPECT_NSEQ(@"Hello world", [pboard findText]);

  [pboard setFindText:original_pboard_text];
}

}  // namespace content
