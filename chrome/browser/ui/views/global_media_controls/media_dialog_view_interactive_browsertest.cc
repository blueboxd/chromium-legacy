// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/image_button.h"

using media_session::mojom::MediaSessionAction;

class MediaDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  MediaDialogViewBrowserTest() = default;
  ~MediaDialogViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  MediaToolbarButtonView* GetToolbarIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->media_button();
  }

  void ClickToolbarIcon() { ClickButton(GetToolbarIcon()); }

  bool IsToolbarIconVisible() {
    base::RunLoop().RunUntilIdle();
    return GetToolbarIcon()->GetVisible();
  }

  void OpenTestURL() {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("media/session")),
        base::FilePath(FILE_PATH_LITERAL("video-with-metadata.html")));
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDifferentMetadataURLInNewTab() {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("media/session")),
        base::FilePath(
            FILE_PATH_LITERAL("video-with-different-metadata.html")));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  void StartPlayback() {
    // The test HTML files used in these tests contain "play()" functions that
    // play the video.
    GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("play()"), base::NullCallback());
  }

  void WaitForStart() {
    content::MediaStartStopObserver observer(
        GetWebContents(), content::MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop() {
    content::MediaStartStopObserver observer(
        GetWebContents(), content::MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  bool IsDialogVisible() {
    base::RunLoop().RunUntilIdle();
    return MediaDialogView::IsShowing();
  }

  bool DialogContainsText(const base::string16& text) {
    return ViewContainsText(MediaDialogView::GetDialogViewForTesting(), text);
  }

  void ClickPauseButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kPause));
  }

  void ClickPlayButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kPlay));
  }

 private:
  void ClickButton(views::Button* button) {
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }

  // Recursively tries to find a views::Label containing |text| within |view|
  // and its children.
  bool ViewContainsText(const views::View* view, const base::string16& text) {
    if (view->GetClassName() == views::Label::kViewClassName) {
      const views::Label* label = static_cast<const views::Label*>(view);
      if (label->GetText().find(text) != std::string::npos)
        return true;
    }

    for (const views::View* child : view->children()) {
      if (ViewContainsText(child, text))
        return true;
    }

    return false;
  }

  views::ImageButton* GetButtonForAction(MediaSessionAction action) {
    return GetButtonForAction(
        MediaDialogView::GetDialogViewForTesting()->children().front(),
        static_cast<int>(action));
  }

  // Recursively tries to find a views::ImageButton for the given
  // MediaSessionAction. This operates under the assumption that
  // media_message_center::MediaNotificationView sets the tags of its action
  // buttons to the MediaSessionAction value.
  views::ImageButton* GetButtonForAction(views::View* view, int action) {
    if (view->GetClassName() == views::ImageButton::kViewClassName) {
      views::ImageButton* image_button = static_cast<views::ImageButton*>(view);
      if (image_button->tag() == action)
        return image_button;
    }

    for (views::View* child : view->children()) {
      views::ImageButton* result = GetButtonForAction(child, action);
      if (result)
        return result;
    }

    return nullptr;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DISALLOW_COPY_AND_ASSIGN(MediaDialogViewBrowserTest);
};

#if defined(OS_MACOSX)
// TODO(https://crbug.com/998342): Fix these on Mac.
#define MAYBE_ShowsMetadataAndControlsMedia \
  DISABLED_ShowsMetadataAndControlsMedia
#else
#define MAYBE_ShowsMetadataAndControlsMedia ShowsMetadataAndControlsMedia
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       MAYBE_ShowsMetadataAndControlsMedia) {
  // The toolbar icon should not start visible.
  EXPECT_FALSE(IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  EXPECT_FALSE(IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  EXPECT_TRUE(IsToolbarIconVisible());
  EXPECT_FALSE(IsDialogVisible());

  // Clicking on the toolbar icon should open the dialog.
  ClickToolbarIcon();
  EXPECT_TRUE(IsDialogVisible());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Big Buck Bunny")));
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Blender Foundation")));

  // Clicking on the pause button in the dialog should pause the media on the
  // page.
  ClickPauseButtonOnDialog();
  WaitForStop();

  // Clicking on the play button in the dialog should play the media on the
  // page.
  ClickPlayButtonOnDialog();
  WaitForStart();

  // Clicking on the toolbar icon again should hide the dialog.
  EXPECT_TRUE(IsDialogVisible());
  ClickToolbarIcon();
  EXPECT_FALSE(IsDialogVisible());
}

#if defined(OS_MACOSX)
// TODO(https://crbug.com/998342): Fix these on Mac.
#define MAYBE_ShowsMultipleMediaSessions DISABLED_ShowsMultipleMediaSessions
#else
#define MAYBE_ShowsMultipleMediaSessions ShowsMultipleMediaSessions
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       MAYBE_ShowsMultipleMediaSessions) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open another tab and play different media.
  OpenDifferentMetadataURLInNewTab();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  ClickToolbarIcon();
  EXPECT_TRUE(IsDialogVisible());

  // The dialog should show both media sessions.
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Big Buck Bunny")));
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Blender Foundation")));
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Different Title")));
  EXPECT_TRUE(DialogContainsText(base::ASCIIToUTF16("Another Artist")));
}
