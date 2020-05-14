// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include <memory>

#include "ash/public/cpp/test/test_tablet_mode.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class TestingAccessibilityHandler : public AccessibilityHandler {
 public:
  explicit TestingAccessibilityHandler(content::WebUI* web_ui)
      : AccessibilityHandler(/* profile */ nullptr) {
    set_web_ui(web_ui);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAccessibilityHandler);
};

class AccessibilityHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AccessibilityHandlerTest() = default;
  AccessibilityHandlerTest(const AccessibilityHandlerTest&) = delete;
  AccessibilityHandlerTest& operator=(const AccessibilityHandlerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_tablet_mode_ = std::make_unique<ash::TestTabletMode>();
    handler_ = std::make_unique<TestingAccessibilityHandler>(&web_ui_);
  }

  void TearDown() override {
    handler_.reset();
    test_tablet_mode_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI web_ui_;
  std::unique_ptr<ash::TestTabletMode> test_tablet_mode_;
  std::unique_ptr<TestingAccessibilityHandler> handler_;
};

// Test that when tablet mode is disabled, the correct data is returned by
// HandleManageA11yPageReady().
TEST_F(AccessibilityHandlerTest, ManageA11yPageReadyTabletModeDisabled) {
  test_tablet_mode_->SetEnabledForTest(false);
  handler_->HandleManageA11yPageReady(/* args */ nullptr);

  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();

  // Ensure tablet mode is returned as disabled.
  EXPECT_FALSE(call_data.arg3()->GetBool());
}

// Test that when tablet mode is enabled, the correct data is returned by
// HandleManageA11yPageReady().
TEST_F(AccessibilityHandlerTest, ManageA11yPageReadyTabletModeEnabled) {
  test_tablet_mode_->SetEnabledForTest(true);
  handler_->HandleManageA11yPageReady(/* args */ nullptr);

  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();

  // Ensure tablet mode is returned as enabled.
  EXPECT_TRUE(call_data.arg3()->GetBool());
}

}  // namespace settings
}  // namespace chromeos
