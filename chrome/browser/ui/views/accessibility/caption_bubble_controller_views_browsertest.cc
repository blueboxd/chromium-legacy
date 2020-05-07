// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/caption.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace captions {

namespace {
// Test constants.
static constexpr int kArrowKeyDisplacement = 16;
}  // namespace

class CaptionBubbleControllerViewsTest : public InProcessBrowserTest {
 public:
  CaptionBubbleControllerViewsTest() = default;
  ~CaptionBubbleControllerViewsTest() override = default;
  CaptionBubbleControllerViewsTest(const CaptionBubbleControllerViewsTest&) =
      delete;
  CaptionBubbleControllerViewsTest& operator=(
      const CaptionBubbleControllerViewsTest&) = delete;

  CaptionBubbleControllerViews* GetController() {
    if (!controller_)
      controller_ = std::make_unique<CaptionBubbleControllerViews>(browser());
    return controller_.get();
  }

  CaptionBubble* GetBubble() {
    return controller_ ? controller_->caption_bubble_ : nullptr;
  }

  views::Label* GetLabel() {
    return controller_ ? controller_->caption_bubble_->label_ : nullptr;
  }

  views::Label* GetTitle() {
    return controller_ ? controller_->caption_bubble_->title_ : nullptr;
  }

  views::Label* GetErrorMessage() {
    return controller_ ? controller_->caption_bubble_->error_message_ : nullptr;
  }

  views::ImageView* GetErrorIcon() {
    return controller_ ? controller_->caption_bubble_->error_icon_ : nullptr;
  }

  std::string GetLabelText() {
    return controller_ ? base::UTF16ToUTF8(GetLabel()->GetText()) : "";
  }

  views::Widget* GetCaptionWidget() {
    return controller_ ? controller_->caption_widget_ : nullptr;
  }

  void ClickCloseButton() {
    CaptionBubble* bubble = GetBubble();
    if (!bubble)
      return;
    views::test::WidgetDestroyedWaiter waiter(GetCaptionWidget());
    bubble->close_button_->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0), gfx::Point(0, 0),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    bubble->close_button_->OnMouseReleased(ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    waiter.Wait();
  }

  // There may be some rounding errors as we do floating point math with ints.
  // Check that points are almost the same.
  void ExpectInBottomCenter(gfx::Rect anchor_bounds, gfx::Rect bubble_bounds) {
    EXPECT_LT(
        abs(bubble_bounds.CenterPoint().x() - anchor_bounds.CenterPoint().x()),
        2);
    EXPECT_EQ(bubble_bounds.bottom(), anchor_bounds.bottom() - 48);
  }

  void OnPartialTranscription(std::string text) {
    GetController()->OnTranscription(
        chrome::mojom::TranscriptionResult::New(text, false));
  }

  void OnFinalTranscription(std::string text) {
    GetController()->OnTranscription(
        chrome::mojom::TranscriptionResult::New(text, true));
  }

 private:
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
};

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsCaptionInBubble) {
  OnPartialTranscription("Taylor");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  EXPECT_EQ("Taylor", GetLabelText());
  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, "
      "1989)");
  EXPECT_EQ("Taylor Alison Swift (born December 13, 1989)", GetLabelText());

  // Hides the bubble when set to the empty string.
  OnPartialTranscription("");
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Shows it again when the caption is no longer empty.
  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, "
      "1989) is an American singer-songwriter.");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  EXPECT_EQ(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter.",
      GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, LaysOutCaptionLabel) {
  // A short caption is bottom-aligned with the bubble. The bubble bounds
  // are inset by 4 dip of margin, add another 2 dip of margin for the label's
  // container bounds to get 6 dip (spec).
  OnPartialTranscription("Cats rock");
  EXPECT_EQ(GetLabel()->GetBoundsInScreen().bottom() + 2,
            GetBubble()->GetBoundsInScreen().bottom());

  // Ensure overflow by using a very long caption, should still be aligned
  // with the bottom of the bubble.
  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.");
  EXPECT_EQ(GetLabel()->GetBoundsInScreen().bottom() + 2,
            GetBubble()->GetBoundsInScreen().bottom());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       CaptionTitleShownAtFirst) {
  // With one line of text, the title is visible and positioned between the
  // top of the bubble and top of the label.
  OnPartialTranscription("Cats rock");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_EQ(GetTitle()->GetBoundsInScreen().bottom(),
            GetLabel()->GetBoundsInScreen().y());

  OnPartialTranscription("Cats rock\nDogs too");
  EXPECT_FALSE(GetTitle()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, BubblePositioning) {
  views::View* contents_view =
      BrowserView::GetBrowserViewForBrowser(browser())->GetContentsView();

  browser()->window()->SetBounds(gfx::Rect(10, 10, 800, 600));
  OnPartialTranscription("Mantis shrimp have 12-16 photoreceptors");
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Move the window and the widget should stay centered.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Shrink the window's height.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 300));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Shrink it super far, then grow it back up again, and it should still
  // be in the right place.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 100));
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 500));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Now shrink the width so that the caption bubble shrinks.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 500, 500));
  gfx::Rect widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  gfx::Rect contents_bounds = contents_view->GetBoundsInScreen();
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_LT(GetBubble()->GetBoundsInScreen().width(), 548);
  EXPECT_EQ(20, widget_bounds.x() - contents_bounds.x());
  EXPECT_EQ(20, contents_bounds.right() - widget_bounds.right());

  // Make it bigger again and ensure it's visible and wide again.
  // Note: On Mac we cannot put the window too close to the top of the screen
  // or it gets pushed down by the menu bar.
  browser()->window()->SetBounds(gfx::Rect(100, 100, 800, 600));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Now move the widget within the window.
  GetCaptionWidget()->SetBounds(
      gfx::Rect(200, 300, GetCaptionWidget()->GetWindowBoundsInScreen().width(),
                GetCaptionWidget()->GetWindowBoundsInScreen().height()));

  // The bubble width should not have changed.
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Move the window and the widget stays fixed with respect to the window.
  browser()->window()->SetBounds(gfx::Rect(100, 100, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(200, widget_bounds.x());
  EXPECT_EQ(300, widget_bounds.y());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), 548);

  // Now put the window in the top corner for easier math.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(150, widget_bounds.x());
  EXPECT_EQ(250, widget_bounds.y());
  contents_bounds = contents_view->GetBoundsInScreen();
  double x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                   (1.0 * contents_bounds.width());
  double y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                   (1.0 * contents_bounds.height());

  // The center point ratio should not change as we resize the window, and the
  // widget is repositioned.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 750, 550));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  double new_x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                       (1.0 * contents_bounds.width());
  double new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                       (1.0 * contents_bounds.height());
  EXPECT_NEAR(x_ratio, new_x_ratio, .005);
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);

  browser()->window()->SetBounds(gfx::Rect(50, 50, 700, 500));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  new_x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                (1.0 * contents_bounds.width());
  new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                (1.0 * contents_bounds.height());
  EXPECT_NEAR(x_ratio, new_x_ratio, .005);
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);

  // But if we make the window too small, the widget will stay within its
  // bounds.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 500, 500));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                (1.0 * contents_bounds.height());
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);
  EXPECT_TRUE(contents_bounds.Contains(widget_bounds));

  // Making it big again resets the position to what it was before.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(150, widget_bounds.x());
  EXPECT_EQ(250, widget_bounds.y());

#if !defined(OS_MACOSX)
  // Shrink it so small the caption bubble can't fit. Ensure it's hidden.
  // Mac windows cannot be shrunk small enough to force the bubble to hide.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Make it bigger again and ensure it's visible and wide again.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
#endif
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsAndHidesError) {
  OnPartialTranscription("Elephants' trunks average 6 feet long.");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());
  EXPECT_FALSE(GetErrorIcon()->GetVisible());

  GetBubble()->SetHasError(true);
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());
  EXPECT_TRUE(GetErrorIcon()->GetVisible());

  // Setting text during an error shouldn't cause the error to disappear.
  OnPartialTranscription("Elephant tails average 4-5 feet long.");
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());
  EXPECT_TRUE(GetErrorIcon()->GetVisible());

  // Clear the error and everything should be visible again.
  GetBubble()->SetHasError(false);
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());
  EXPECT_FALSE(GetErrorIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, CloseButtonCloses) {
  OnPartialTranscription("Elephants have 3-4 toenails per foot");
  EXPECT_TRUE(GetCaptionWidget());
  ClickCloseButton();
  EXPECT_FALSE(GetCaptionWidget());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       MovesWithArrowsWhenFocused) {
  OnPartialTranscription("Nearly all ants are female.");
  // Not focused initially.
  EXPECT_FALSE(GetBubble()->HasFocus());

  // Key presses do not change the bounds when it is not focused.
  gfx::Rect bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

  // Focus the bubble, and try the arrow keys.
  GetBubble()->RequestFocus();
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  bounds.Offset(0, -kArrowKeyDisplacement);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  bounds.Offset(-kArrowKeyDisplacement, 0);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RIGHT, false,
                                              false, false, false));
  bounds.Offset(kArrowKeyDisplacement, 0);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  bounds.Offset(0, kArrowKeyDisplacement);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

  // Down shouldn't move the bubble again because we started at the bottom of
  // the screen.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

#if !defined(OS_MACOSX)
  // TODO(crbug.com/1055150): Get this working for Mac.
  // Hitting the escape key should remove focus from the view, so arrows no
  // longer work.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
#endif
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionTextSize) {
  int textSize = 16;
  int lineHeight = 24;
  int bubbleHeight = 48;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription("Hamsters' teeth never stop growing");
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the text size to 200%.
  ui::CaptionStyle caption_style;
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize * 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize * 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight * 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight * 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight * 2);

  // Set the text size to the empty string.
  caption_style.text_size = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the text size to 50% !important.
  caption_style.text_size = "50% !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize / 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize / 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight / 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight / 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight / 2);

  // Set the text size to a bad string.
  caption_style.text_size = "Ostriches can run up to 45mph";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the caption style to nullopt.
  GetController()->UpdateCaptionStyle(base::nullopt);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the error message.
  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  GetBubble()->SetHasError(true);
  EXPECT_EQ(lineHeight / 2, GetErrorMessage()->GetLineHeight());
  // The error icon height remains unchanged at 20.
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), lineHeight / 2 + 20);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       PartialAndFinalTranscriptions) {
  OnPartialTranscription("No");
  EXPECT_EQ("No", GetLabelText());
  OnPartialTranscription("No human");
  EXPECT_EQ("No human", GetLabelText());
  OnFinalTranscription("No human has ever seen");
  EXPECT_EQ("No human has ever seen", GetLabelText());
  OnFinalTranscription(" a living");
  EXPECT_EQ("No human has ever seen a living", GetLabelText());
  OnPartialTranscription(" giant");
  EXPECT_EQ("No human has ever seen a living giant", GetLabelText());
  OnPartialTranscription("");
  EXPECT_EQ("No human has ever seen a living", GetLabelText());
  OnPartialTranscription(" giant squid");
  EXPECT_EQ("No human has ever seen a living giant squid", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsAndHidesBubble) {
  // Bubble isn't shown when controller is created.
  GetController();
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // It is shown if there is an error, and hidden when that error goes away.
  GetBubble()->SetHasError(true);
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  GetBubble()->SetHasError(false);
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // It is shown if there is text, and hidden if the text is removed.
  OnPartialTranscription("Newborn kangaroos are less than 1 in long");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  // Stays visible when switching to an error state.
  GetBubble()->SetHasError(true);
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  // Even if the text is removed, because of the error.
  OnFinalTranscription("");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  // No error and no text means not visible.
  GetBubble()->SetHasError(false);
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Explicitly tell the bubble to show itself. It shouldn't show because
  // it has no text and no error.
  GetBubble()->Show();
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());
  GetBubble()->SetHasError(true);
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());

  // Telling it explicitly to hide will hide it.
  GetBubble()->Hide();
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());
  GetBubble()->Show();
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());

#if !defined(OS_MACOSX)
  // Shrink it so small the caption bubble can't fit. Ensure it's hidden.
  // Mac windows cannot be shrunk small enough to force the bubble to hide.
  GetBubble()->SetHasError(false);
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Make it bigger again and ensure it's still not visible.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Now set some text, and ensure it hides when shrunk but re-shows when
  // grown.
  OnPartialTranscription("Newborn opossums are about 1cm long");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
#endif
}

}  // namespace captions
