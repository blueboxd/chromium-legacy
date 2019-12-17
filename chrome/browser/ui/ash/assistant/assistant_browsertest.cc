// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace chromeos {
namespace assistant {

class AssistantBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantBrowserTest() = default;
  ~AssistantBrowserTest() override = default;

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  AssistantTestMixin* tester() { return &tester_; }

 private:
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(),
                             FakeS3Mode::kReplay};

  DISALLOW_COPY_AND_ASSIGN(AssistantBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       ShouldOpenAssistantUiWhenPressingAssistantKey) {
  tester()->StartAssistantAndWaitForReady();

  tester()->PressAssistantKey();

  EXPECT_TRUE(tester()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldDisplayTextResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  tester()->SendTextQuery("test");
  tester()->ExpectAnyOfTheseTextResponses({
      "No one told me there would be a test",
      "You're coming in loud and clear",
      "debug OK",
      "I can assure you, this thing's on",
      "Is this thing on?",
  });
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldDisplayCardResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("What is the highest mountain in the world?");
  tester()->ExpectCardResponse("Mount Everest");
}

}  // namespace assistant
}  // namespace chromeos
