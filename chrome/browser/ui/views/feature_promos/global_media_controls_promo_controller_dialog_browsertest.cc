// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/feature_promos/global_media_controls_promo_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "media/base/media_switches.h"

class GlobalMediaControlsPromoControllerDialogBrowserTest
    : public DialogBrowserTest {
 public:
  GlobalMediaControlsPromoControllerDialogBrowserTest() = default;

  void SetUp() override {
    // Need to enable GMC before the browser is built so that the toolbar is
    // constructed properly.
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControls);
    DialogBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    promo_controller_ = std::make_unique<GlobalMediaControlsPromoController>(
        GetMediaToolbarButton(), browser()->profile());
  }

  void ShowUi(const std::string& name) override { ShowPromo(); }

  void ShowPromo() {
    ShowMediaToolbarButton();
    promo_controller_->ShowPromo();
  }

  void DisableBubbleTimeout() {
    promo_controller_->disable_bubble_timeout_for_test();
  }

  bool PromoBubbleVisible() {
    views::View* bubble = promo_controller_->promo_bubble_for_test();
    return bubble && bubble->GetVisible();
  }

  // TODO(https://crbug.com/991585): Once we add the proper logic into
  // MediaToolbarButtonController, call methods on that instead of directly
  // calling GlobalMediaControlsPromoController methods.
  void SimulateMediaDialogOpened() { promo_controller_->OnMediaDialogOpened(); }

  void SimulateMediaToolbarButtonDisabledOrHidden() {
    promo_controller_->OnMediaToolbarButtonDisabledOrHidden();
  }

 private:
  MediaToolbarButtonView* GetMediaToolbarButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->media_button();
  }

  // Forces the media toolbar button to appear so we can add the promo to it.
  void ShowMediaToolbarButton() {
    auto* media_button = GetMediaToolbarButton();
    media_button->Show();
    media_button->Enable();
  }

  std::unique_ptr<GlobalMediaControlsPromoController> promo_controller_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       InvokeUi_default) {
  DisableBubbleTimeout();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       BubbleHidesAfter5Seconds) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  ShowPromo();

  // The promo should hide after 5 seconds.
  EXPECT_TRUE(PromoBubbleVisible());
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(4900));
  EXPECT_TRUE(PromoBubbleVisible());
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(PromoBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       BubbleHidesIfTheMediaDialogIsOpened) {
  DisableBubbleTimeout();
  ShowPromo();

  EXPECT_TRUE(PromoBubbleVisible());

  SimulateMediaDialogOpened();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(PromoBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       BubbleHidesIfTheMediaToolbarButtonIsDisabledOrHidden) {
  DisableBubbleTimeout();
  ShowPromo();

  EXPECT_TRUE(PromoBubbleVisible());

  SimulateMediaToolbarButtonDisabledOrHidden();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(PromoBubbleVisible());
}
