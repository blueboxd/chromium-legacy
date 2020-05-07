// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

enum SpokenFeedbackAppListTestVariant { kTestAsNormalUser, kTestAsGuestUser };

class TestSuggestionChipResult : public ash::TestSearchResult {
 public:
  explicit TestSuggestionChipResult(const base::string16& title) {
    set_display_type(ash::SearchResultDisplayType::kChip);
    set_title(title);
  }
  ~TestSuggestionChipResult() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSuggestionChipResult);
};

class SpokenFeedbackAppListTest
    : public LoggedInSpokenFeedbackTest,
      public ::testing::WithParamInterface<SpokenFeedbackAppListTestVariant> {
 protected:
  SpokenFeedbackAppListTest() = default;
  ~SpokenFeedbackAppListTest() override = default;

  void SetUp() override {
    // Do not run expand arrow hinting animation to avoid msan test crash.
    // (See https://crbug.com/926038)
    ash::AppListView::SetShortAnimationForTesting(true);
    LoggedInSpokenFeedbackTest::SetUp();
  }

  void TearDown() override {
    LoggedInSpokenFeedbackTest::TearDown();
    ash::AppListView::SetShortAnimationForTesting(false);
  }

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();
    auto* controller = ash::Shell::Get()->app_list_controller();
    controller->SetSearchTabletAndClamshellAccessibleName(
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME_TABLET),
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME));
    controller->SetAppListModelForTest(
        std::make_unique<ash::test::AppListTestModel>());
    app_list_test_model_ =
        static_cast<ash::test::AppListTestModel*>(controller->GetModel());
    search_model = controller->GetSearchModel();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == kTestAsGuestUser) {
      command_line->AppendSwitch(chromeos::switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      "user");
      command_line->AppendSwitchASCII(
          switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
    }
  }

  // Populate apps grid with |num| items.
  void PopulateApps(size_t num) { app_list_test_model_->PopulateApps(num); }

  // Populate |num| suggestion chips.
  void PopulateChips(size_t num) {
    for (size_t i = 0; i < num; i++) {
      search_model->results()->Add(std::make_unique<TestSuggestionChipResult>(
          base::UTF8ToUTF16("Chip " + base::NumberToString(i))));
    }
  }

 private:
  ash::test::AppListTestModel* app_list_test_model_ = nullptr;
  ash::SearchModel* search_model = nullptr;
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         SpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, LauncherStateTransition) {
  EnableChromeVox();

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.ExpectSpeech("Edit text");
  // Check that Launcher, partial view state is announced.
  sm_.ExpectSpeech("Launcher, partial view");
  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  // Move focus to expand all apps button;
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");
  sm_.ExpectSpeech("Button");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech(
      "Search your device, apps, and web."
      " Use the arrow keys to navigate your apps.");
  sm_.ExpectSpeech("Edit text");
  // Check that Launcher, all apps state is announced.
  sm_.ExpectSpeech("Launcher, all apps");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       DisabledFullscreenExpandButton) {
  EnableChromeVox();

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeech("Shelf");

  // Press space on the launcher button in shelf, this opens peeking launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");

  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });

  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");

  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Make sure the first traversal left is not the expand arrow button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNot("Expand to all apps");

  // Make sure the second traversal left is not the expand arrow button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNot("Expand to all apps");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       PeekingLauncherFocusTraversal) {
  // Add 3 suggestion chips.
  PopulateChips(3);

  EnableChromeVox();

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to 1st suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 2nd suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 1");
  sm_.ExpectSpeech("Button");
  // Move focus to 3rd suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 2");
  sm_.ExpectSpeech("Button");
  // Move focus to expand all apps button;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Expand to all apps");
  sm_.ExpectSpeech("Button");
  // Move focus to app list window;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       FullscreenLauncherFocusTraversal) {
  // Add 1 suggestion chip and 3 apps.
  PopulateChips(1);
  PopulateApps(3);

  EnableChromeVox();

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");
  // Move focus to the suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 2nd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 1");
  sm_.ExpectSpeech("Button");
  // Move focus to 3rd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 2");
  sm_.ExpectSpeech("Button");
  // Move focus to app list window;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech(
      "Search your device, apps, and web. Use the arrow keys to navigate "
      "your "
      "apps.");
  // Move focus to search box;
  sm_.ExpectSpeech("Edit text");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, NavigateAppLauncher) {
  EnableChromeVox();

  sm_.Call([this]() {
    // Add one app to the applist.
    PopulateApps(1);

    EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  });

  // Wait for it to say "Launcher", "Button", "Shelf", "Tool bar".
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Click on the launcher, it brings up the app list UI.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm_.ExpectSpeech(
      "Search your device, apps, and web. Use the arrow keys to navigate your "
      "apps.");
  sm_.ExpectSpeech("Edit text");

  // Close it and open it again.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm_.ExpectSpeechPattern("*window*");

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm_.ExpectSpeech(
      "Search your device, apps, and web. Use the arrow keys to navigate your "
      "apps.");

  // Now press the right arrow and we should be focused on an app button
  // in a dialog.
  // THis doesn't work though (to be done below).

  // TODO(newcomer): reimplement this test once the AppListFocus changes are
  // complete (http://crbug.com/784942).

  sm_.Replay();
}

}  // namespace chromeos
