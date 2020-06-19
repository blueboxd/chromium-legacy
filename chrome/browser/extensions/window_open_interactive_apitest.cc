// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

// http://crbug.com/253417 for NDEBUG
#if (defined(OS_WIN) || defined(OS_MACOSX)) && defined(NDEBUG)
// Focus test fails if there is no window manager on Linux.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenFocus) {
  ASSERT_TRUE(RunExtensionTest("window_open/focus")) << message_;
}
#endif

// The test uses the chrome.browserAction.openPopup API, which requires that the
// window can automatically be activated.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpen) {
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionIncognito(
      test_data_dir_.AppendASCII("window_open").AppendASCII("spanning")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
