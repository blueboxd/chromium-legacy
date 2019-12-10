// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using OmniboxFocusInteractiveTest = ExtensionBrowserTest;

// Verify that an NTP-replacement extension results in the NTP web contents
// being focused - this is a regression test for https://crbug.com/1027719.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest, NtpReplacementExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that
  // 1) provides a replacement for chrome://newtab URL
  // 2) navigates away from the replacement
  TestExtensionDir dir;
  const char kManifest[] = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext_ntp.html"
        },
        "manifest_version": 2,
        "name": "NTP-replacement extension",
        "version": "1.0"
      } )";
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("ext_ntp.html"),
                "<script src='ext_ntp.js'></script>");
  GURL final_ntp_url = embedded_test_server()->GetURL("/title1.html");
  dir.WriteFile(FILE_PATH_LITERAL("ext_ntp.js"),
                content::JsReplace("window.location = $1", final_ntp_url));
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Open the new tab, because of the NTP extension behavior, the focus should
  // move to the tab contents.
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(final_ntp_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

}  // namespace extensions
