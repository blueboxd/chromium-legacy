// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/run_loop.h"

namespace ash {

class AssistantPageViewTest : public AssistantAshTestBase {
 public:
  AssistantPageViewTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantPageViewTest);
};

TEST_F(AssistantPageViewTest, ShouldStartAtMinimumHeight) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchBox);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest,
       ShouldRemainAtMinimumHeightWhenDisplayingOneLiner) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchBox);

  MockAssistantInteractionWithResponse("Short one-liner");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWithMultilineText) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchBox);

  MockAssistantInteractionWithResponse(
      "This\ntext\nhas\na\nlot\nof\nlinebreaks.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMaxHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWhenWrappingTextLine) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchBox);

  MockAssistantInteractionWithResponse(
      "This is a very long text without any linebreaks. "
      "This will wrap, and should cause the Assistant view to get bigger. "
      "If it doesn't, this looks really bad. This is what caused b/134963994.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMaxHeightEmbeddedDip, main_view()->size().height());
}

}  // namespace ash
