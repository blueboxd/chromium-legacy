// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class ProcessInternalsWebUiBrowserTest : public ContentBrowserTest {};

// This test verifies that loading of the process-internals WebUI works
// correctly and the process rendering it has no WebUI bindings.
IN_PROC_BROWSER_TEST_F(ProcessInternalsWebUiBrowserTest, NoProcessBindings) {
  GURL url("chrome://process-internals/#web-contents");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));

  // Execute script to ensure the page has loaded correctly and was successful
  // at retrieving data from the browser process.
  // Note: This requires using an isolated world in which to execute the
  // script because WebUI has a default CSP policy denying "eval()", which is
  // what EvalJs uses under the hood.
  std::string page_contents =
      EvalJs(shell()->web_contents()->GetMainFrame(), "document.body.innerHTML",
             EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */)
          .ExtractString();

  // Crude verification that the page had the right content.
  EXPECT_THAT(page_contents, ::testing::HasSubstr("Process Internals"));
}

}  // namespace content
