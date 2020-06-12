// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "net/cookies/cookie_util.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

// TODO(crbug.com/1093066): This test uses the DOM to set and
// check cookies for one test. Figure out how to isolate that
// test and adapt the rest of it for a SW-based extension.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Cookies) {
  ASSERT_TRUE(RunExtensionTestWithArg(
      "cookies/api",
      net::cookie_util::IsCookiesWithoutSameSiteMustBeSecureEnabled()
          ? "true"
          : "false"))
      << message_;
}

class CookiesApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 public:
  void SetUp() override {
    ExtensionApiTest::SetUp();
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }

 protected:
  bool RunTest(const std::string& extension_name) {
    return RunTestWithFlags(extension_name, kFlagNone);
  }

  bool RunTestIncognito(const std::string& extension_name) {
    return RunTestWithFlags(extension_name, kFlagEnableIncognito);
  }

  bool RunTestWithFlags(const std::string& extension_name,
                        int browser_test_flags) {
    if (GetParam() == ContextType::kServiceWorker)
      browser_test_flags |= kFlagRunAsServiceWorkerBasedExtension;

    return RunExtensionTestWithFlags(extension_name, browser_test_flags,
                                     kFlagNone);
  }

  std::unique_ptr<ScopedWorkerBasedExtensionsChannel> current_channel_;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEvents) {
  ASSERT_TRUE(RunTest("cookies/events")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEventsSpanning) {
  // We need to initialize an incognito mode window in order have an initialized
  // incognito cookie store. Otherwise, the chrome.cookies.set operation is just
  // ignored and we won't be notified about a newly set cookie for which we want
  // to test whether the storeId is set correctly.
  OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
  ASSERT_TRUE(RunTestIncognito("cookies/events_spanning")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunTest("cookies/no_permission")) << message_;
}

}  // namespace extensions
