// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/test/browser_test.h"

class PermissionPromptBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  PermissionPromptBubbleViewBrowserTest() = default;

  PermissionPromptBubbleViewBrowserTest(
      const PermissionPromptBubbleViewBrowserTest&) = delete;
  PermissionPromptBubbleViewBrowserTest& operator=(
      const PermissionPromptBubbleViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());
    test_api_->AddSimpleRequest(ContentSettingsType::GEOLOCATION);

    base::RunLoop().RunUntilIdle();
  }
};

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}
