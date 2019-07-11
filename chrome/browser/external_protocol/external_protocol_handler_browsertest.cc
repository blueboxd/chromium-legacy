// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"

class ExternalProtocolHandlerBrowserTest : public InProcessBrowserTest {};

const char kScript[] =
    "new Promise(res => {"
    "   const tab = window.open('mailto:test@site.test', '_blank');"
    "   tab.addEventListener('unload', () => {"
    "     res(true);"
    "   });"
    "});";

// http://crbug.com/982234
#if defined(OS_WIN)
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation \
  DISABLED_AutoCloseTabOnNonWebProtocolNavigation
#else
#define MAYBE_AutoCloseTabOnNonWebProtocolNavigation \
  AutoCloseTabOnNonWebProtocolNavigation
#endif
IN_PROC_BROWSER_TEST_F(ExternalProtocolHandlerBrowserTest,
                       MAYBE_AutoCloseTabOnNonWebProtocolNavigation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, EvalJs(web_contents, kScript));
}
