// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include "ash/shell.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_accessibility_helper_instance.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/viz/common/features.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

class ArcAccessibilityHelperBridgeBrowserTest : public InProcessBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    fake_accessibility_helper_instance_ =
        std::make_unique<FakeAccessibilityHelperInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->SetInstance(fake_accessibility_helper_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->accessibility_helper());

    chromeos::AccessibilityManager::Get()->SetProfileForTest(
        browser()->profile());

    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
    exo::WMHelper::SetInstance(wm_helper_.get());
  }

  void TearDownOnMainThread() override {
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();

    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->CloseInstance(fake_accessibility_helper_instance_.get());
    fake_accessibility_helper_instance_.reset();
  }

 protected:
  std::unique_ptr<FakeAccessibilityHelperInstance>
      fake_accessibility_helper_instance_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       PreferenceChange) {
  ASSERT_EQ(mojom::AccessibilityFilterType::OFF,
            fake_accessibility_helper_instance_->filter_type());
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  exo::test::ExoTestHelper exo_test_helper;
  exo::test::ExoTestWindow test_window_1 =
      exo_test_helper.CreateWindow(640, 480, false /* is_modal */);
  exo::test::ExoTestWindow test_window_2 =
      exo_test_helper.CreateWindow(640, 480, false /* is_modal */);

  // Forcefully set task_id to each window.
  exo::SetShellApplicationId(
      test_window_1.shell_surface()->GetWidget()->GetNativeWindow(),
      "org.chromium.arc.1");
  exo::SetShellApplicationId(
      test_window_2.shell_surface()->GetWidget()->GetNativeWindow(),
      "org.chromium.arc.2");

  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();
  activation_client->ActivateWindow(
      test_window_1.shell_surface()->GetWidget()->GetNativeWindow());
  ASSERT_EQ(test_window_1.shell_surface()->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  ASSERT_FALSE(
      test_window_1.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));
  ASSERT_FALSE(
      test_window_2.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Confirm that filter type is updated with preference change.
  EXPECT_EQ(mojom::AccessibilityFilterType::ALL,
            fake_accessibility_helper_instance_->filter_type());

  // Use ChromeVox by default. Touch exploration pass through is still false.
  EXPECT_FALSE(
      test_window_1.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());

  // Enable TalkBack. Touch exploration pass through of test_window_1
  // (current active window) would become true.
  bridge->SetNativeChromeVoxArcSupport(false);

  EXPECT_TRUE(test_window_1.shell_surface()
                  ->GetWidget()
                  ->GetNativeWindow()
                  ->GetProperty(
                      aura::client::kAccessibilityTouchExplorationPassThrough));

  // Activate test_window_2 and confirm that it still be false.
  activation_client->ActivateWindow(
      test_window_2.shell_surface()->GetWidget()->GetNativeWindow());
  ASSERT_EQ(test_window_2.shell_surface()->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  EXPECT_FALSE(
      test_window_2.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));

  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       ExploreByTouchMode) {
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);
  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  // Check that explore by touch doesn't get disabled as long as ChromeVox
  // remains enabled.
  chromeos::AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(false);
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  chromeos::AccessibilityManager::Get()->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());
}

}  // namespace arc
