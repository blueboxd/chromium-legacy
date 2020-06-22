// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

namespace captions {

class CaptionControllerTest : public InProcessBrowserTest {
 public:
  CaptionControllerTest() = default;
  ~CaptionControllerTest() override = default;
  CaptionControllerTest(const CaptionControllerTest&) = delete;
  CaptionControllerTest& operator=(const CaptionControllerTest&) = delete;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(media::kLiveCaption);
    InProcessBrowserTest::SetUp();
  }

  void SetLiveCaptionEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 enabled);
  }

  CaptionController* GetController() {
    return CaptionControllerFactory::GetForProfile(browser()->profile());
  }

  CaptionBubbleController* GetBubbleController() {
    return GetBubbleControllerForBrowser(browser());
  }

  CaptionBubbleController* GetBubbleControllerForBrowser(Browser* browser) {
    return GetController()->GetCaptionBubbleControllerForBrowser(browser);
  }

  bool DispatchTranscription(std::string text) {
    return DispatchTranscriptionToBrowser(text, browser());
  }

  bool DispatchTranscriptionToBrowser(std::string text, Browser* browser) {
    return GetController()->DispatchTranscription(
        browser->tab_strip_model()->GetActiveWebContents(),
        chrome::mojom::TranscriptionResult::New(text, true /* is_final */));
  }

  int NumBubbleControllers() {
    return GetController()->caption_bubble_controllers_.size();
  }

  bool IsWidgetVisible() { return IsWidgetVisibleOnBrowser(browser()); }

  bool IsWidgetVisibleOnBrowser(Browser* browser) {
    return GetBubbleControllerForBrowser(browser)->IsWidgetVisibleForTesting();
  }

  std::string GetBubbleLabelText() {
    return GetBubbleLabelTextOnBrowser(browser());
  }

  std::string GetBubbleLabelTextOnBrowser(Browser* browser) {
    return GetBubbleControllerForBrowser(browser)
        ->GetBubbleLabelTextForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, ProfilePrefsAreRegistered) {
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_EQ(base::FilePath(),
            browser()->profile()->GetPrefs()->GetFilePath(prefs::kSODAPath));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, LiveCaptionEnabledChanged) {
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  EXPECT_NE(nullptr, GetBubbleController());
  EXPECT_EQ(1, NumBubbleControllers());

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       LiveCaptionEnabledChanged_BubbleVisible) {
  SetLiveCaptionEnabled(true);
  // Make the bubble visible by dispatching a transcription.
  DispatchTranscription(
      "In Switzerland it is illegal to own just one guinea pig.");
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisible());
#else
  EXPECT_FALSE(IsWidgetVisible());
#endif

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserAdded) {
  EXPECT_EQ(0, NumBubbleControllers());

  // Add a new browser and then enable live caption. Test that a caption bubble
  // controller is created.
  CreateBrowser(browser()->profile());
  SetLiveCaptionEnabled(true);
  EXPECT_EQ(2, NumBubbleControllers());

  // Add a new browser and test that a caption bubble controller is created.
  CreateBrowser(browser()->profile());
  EXPECT_EQ(3, NumBubbleControllers());

  // Disable live caption. Add a new browser and test that a caption bubble
  // controller is not created.
  SetLiveCaptionEnabled(false);
  CreateBrowser(browser()->profile());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserRemoved) {
  CaptionController* controller = GetController();
  Browser* browser1 = browser();
  // Add 3 browsers.
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* browser3 = CreateBrowser(browser()->profile());
  Browser* browser4 = CreateBrowser(browser()->profile());

  SetLiveCaptionEnabled(true);
  EXPECT_EQ(4, NumBubbleControllers());

  // Close browser4 and test that the caption bubble controller was destroyed.
  browser4->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr, GetBubbleControllerForBrowser(browser4));

  // Make the bubble on browser3 visible by dispatching a transcription.
  DispatchTranscriptionToBrowser(
      "If you lift a kangaroo's tail off the ground it can't hop.", browser3);
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisibleOnBrowser(browser3));
#else
  EXPECT_FALSE(IsWidgetVisibleOnBrowser(browser3));
#endif
  browser3->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr, GetBubbleControllerForBrowser(browser3));

  // Make the bubble on browser2 visible by dispatching a transcription.
  DispatchTranscriptionToBrowser(
      "A lion's roar can be heard from 5 miles away.", browser2);
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisibleOnBrowser(browser2));
#else
  EXPECT_FALSE(IsWidgetVisibleOnBrowser(browser2));
#endif

  // Close all browsers and verify that the caption bubbles are destroyed on
  // the two remaining browsers.
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr,
            controller->GetCaptionBubbleControllerForBrowser(browser2));
  EXPECT_EQ(nullptr,
            controller->GetCaptionBubbleControllerForBrowser(browser1));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, DispatchTranscription) {
  bool success = DispatchTranscription("A baby spider is called a spiderling.");
  EXPECT_FALSE(success);
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  success = DispatchTranscription(
      "A baby octopus is about the size of a flea when it is born.");
  EXPECT_TRUE(success);
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A baby octopus is about the size of a flea when it is born.",
            GetBubbleLabelText());
#else
  EXPECT_FALSE(IsWidgetVisible());
#endif

  SetLiveCaptionEnabled(false);
  success = DispatchTranscription(
      "Approximately 10-20% of power outages in the US are caused by "
      "squirrels.");
  EXPECT_FALSE(success);
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       DispatchTranscription_MultipleBrowsers) {
  SetLiveCaptionEnabled(true);

  // Dispatch transcription routes the transcription to the right browser.
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  bool success = DispatchTranscriptionToBrowser(
      "Honeybees can recognize human faces.", browser1);
  EXPECT_TRUE(success);
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisibleOnBrowser(browser1));
  EXPECT_EQ("Honeybees can recognize human faces.",
            GetBubbleLabelTextOnBrowser(browser1));
  EXPECT_FALSE(IsWidgetVisibleOnBrowser(browser2));
  EXPECT_NE("Honeybees can recognize human faces.",
            GetBubbleLabelTextOnBrowser(browser2));
#else
  EXPECT_FALSE(IsWidgetVisibleOnBrowser(browser1));
#endif

  success = DispatchTranscriptionToBrowser(
      "A blue whale's heart is the size of a small car.", browser2);
  EXPECT_TRUE(success);
#if defined(TOOLKIT_VIEWS)  // CaptionBubbleController only implemented in Views
  EXPECT_TRUE(IsWidgetVisibleOnBrowser(browser2));
  EXPECT_EQ("A blue whale's heart is the size of a small car.",
            GetBubbleLabelTextOnBrowser(browser2));
  EXPECT_EQ("Honeybees can recognize human faces.",
            GetBubbleLabelTextOnBrowser(browser1));
#else
  EXPECT_FALSE(IsWidgetVisibleOnBrowser(browser2));
#endif

  // DispatchTranscription returns false for browsers on different profiles.
  Browser* incognito_browser = CreateIncognitoBrowser();
  success = DispatchTranscriptionToBrowser(
      "Squirrels forget where they hide about half of their nuts.",
      incognito_browser);
  EXPECT_FALSE(success);
  EXPECT_EQ("Honeybees can recognize human faces.",
            GetBubbleLabelTextOnBrowser(browser1));
  EXPECT_EQ("A blue whale's heart is the size of a small car.",
            GetBubbleLabelTextOnBrowser(browser2));
}

}  // namespace captions
