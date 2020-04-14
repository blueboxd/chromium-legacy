// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace captions {

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

  std::string GetLabelText() {
    return controller_ ? base::UTF16ToUTF8(GetLabel()->GetText()) : "";
  }

  views::Widget* GetCaptionWidget() {
    return controller_ ? controller_->caption_widget_ : nullptr;
  }

 private:
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
};

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsCaptionInBubble) {
  GetController()->OnCaptionReceived("Taylor");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  EXPECT_EQ("Taylor", GetLabelText());
  GetController()->OnCaptionReceived(
      "Taylor Alison Swift (born December 13, "
      "1989)");
  EXPECT_EQ("Taylor Alison Swift (born December 13, 1989)", GetLabelText());

  // Hides the bubble when set to the empty string.
  GetController()->OnCaptionReceived("");
  EXPECT_FALSE(GetCaptionWidget()->IsVisible());

  // Shows it again when the caption is no longer empty.
  GetController()->OnCaptionReceived(
      "Taylor Alison Swift (born December 13, "
      "1989) is an American singer-songwriter.");
  EXPECT_TRUE(GetCaptionWidget()->IsVisible());
  EXPECT_EQ(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter.",
      GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, LaysOutCaptionLabel) {
  // A short caption is bottom-aligned with the bubble.
  GetController()->OnCaptionReceived("Cats rock");
  EXPECT_EQ(GetLabel()->GetBoundsInScreen().bottom(),
            GetBubble()->GetBoundsInScreen().bottom());

  // Ensure overflow by using a very long caption, should still be aligned
  // with the bottom of the bubble.
  GetController()->OnCaptionReceived(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.");
  EXPECT_EQ(GetLabel()->GetBoundsInScreen().bottom(),
            GetBubble()->GetBoundsInScreen().bottom());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       CaptionTitleShownAtFirst) {
  // With one line of text, the title is visible and positioned between the
  // top of the bubble and top of the label.
  GetController()->OnCaptionReceived("Cats rock");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_EQ(GetTitle()->GetBoundsInScreen().bottom(),
            GetLabel()->GetBoundsInScreen().y());

  GetController()->OnCaptionReceived("Cats rock\nDogs too");

  EXPECT_FALSE(GetTitle()->GetVisible());
}
}  // namespace captions
