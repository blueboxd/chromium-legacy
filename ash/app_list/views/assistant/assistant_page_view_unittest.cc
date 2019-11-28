// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/run_loop.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

namespace {

using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionType;

#define EXPECT_INTERACTION_OF_TYPE(type_)                      \
  ({                                                           \
    base::Optional<AssistantInteractionMetadata> interaction = \
        current_interaction();                                 \
    ASSERT_TRUE(interaction.has_value());                      \
    EXPECT_EQ(interaction->type, type_);                       \
  })

// Ensures that the given view has the focus. If it doesn't, this will print a
// nice error message indicating which view has the focus instead.
#define EXPECT_HAS_FOCUS(expected_)                                           \
  ({                                                                          \
    const views::View* actual =                                               \
        main_view()->GetFocusManager()->GetFocusedView();                     \
    EXPECT_TRUE(expected_->HasFocus())                                        \
        << "Expected focus on '" << expected_->GetClassName()                 \
        << "' but it is on '" << (actual ? actual->GetClassName() : "<null>") \
        << "'.";                                                              \
  })

// Stubbed |FocusChangeListener| that simply remembers all the views that
// received focus.
class FocusChangeListenerStub : public views::FocusChangeListener {
 public:
  explicit FocusChangeListenerStub(views::View* view)
      : focus_manager_(view->GetFocusManager()) {
    focus_manager_->AddFocusChangeListener(this);
  }
  ~FocusChangeListenerStub() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    focused_views_.push_back(focused_now);
  }

  // Returns all views that received the focus at some point.
  const std::vector<views::View*>& focused_views() const {
    return focused_views_;
  }

 private:
  std::vector<views::View*> focused_views_;
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusChangeListenerStub);
};

// Simply constructs a GestureEvent for test.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(const gfx::Point& location,
                      ui::GestureEventDetails details)
      : GestureEvent(location.x(),
                     location.y(),
                     /*flags=*/ui::EF_NONE,
                     base::TimeTicks(),
                     details) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

class AssistantPageViewTest : public AssistantAshTestBase {
 public:
  AssistantPageViewTest() = default;

  void ShowAssistantUiInTextMode() {
    ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  }

  void ShowAssistantUiInVoiceMode() {
    ShowAssistantUi(AssistantEntryPoint::kHotword);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantPageViewTest);
};

}  // namespace

TEST_F(AssistantPageViewTest, ShouldStartAtMinimumHeight) {
  ShowAssistantUi();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest,
       ShouldRemainAtMinimumHeightWhenDisplayingOneLiner) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse("Short one-liner");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWithMultilineText) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse(
      "This\ntext\nhas\na\nlot\nof\nlinebreaks.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMaxHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWhenWrappingTextLine) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse(
      "This is a very long text without any linebreaks. "
      "This will wrap, and should cause the Assistant view to get bigger. "
      "If it doesn't, this looks really bad. This is what caused b/134963994.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kMaxHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldNotRequestFocusWhenOtherAppWindowOpens) {
  // This tests the root cause of b/141945964.
  // Namely, the Assistant code should not request the focus while being closed.
  ShowAssistantUi();

  // Start observing all focus changes.
  FocusChangeListenerStub focus_listener(main_view());

  // Steal the focus by creating a new App window.
  SwitchToNewAppWindow();

  // This causes the Assistant UI to close.
  ASSERT_FALSE(window()->IsVisible());

  // Now check that no child view of our UI received the focus.
  for (const views::View* view : focus_listener.focused_views()) {
    EXPECT_FALSE(page_view()->Contains(view))
        << "Focus was received by Assistant view '" << view->GetClassName()
        << "' while Assistant was closing";
  }
}

TEST_F(AssistantPageViewTest, ShouldFocusTextFieldWhenOpeningWithHotkey) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, ShouldNotLoseTextfieldFocusWhenSendingTextQuery) {
  ShowAssistantUi();

  SendQueryThroughTextField("The query");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest,
       ShouldNotLoseTextfieldFocusWhenDisplayingResponse) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse("The response");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, ShouldNotLoseTextfieldFocusWhenResizing) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse(
      "This\ntext\nis\nbig\nenough\nto\ncause\nthe\nassistant\nscreen\nto\n"
      "resize.");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, ShouldFocusMicWhenOpeningWithHotword) {
  ShowAssistantUi(AssistantEntryPoint::kHotword);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTest, ShouldShowGreetingLabelWhenOpening) {
  ShowAssistantUi();

  EXPECT_TRUE(greeting_label()->GetVisible());
}

TEST_F(AssistantPageViewTest, ShouldHideGreetingLabelAfterQuery) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse("The response");

  EXPECT_FALSE(greeting_label()->GetVisible());
}

TEST_F(AssistantPageViewTest, ShouldShowGreetingLabelAgainAfterReopening) {
  ShowAssistantUi();

  // Cause the label to be hidden.
  MockAssistantInteractionWithResponse("The response");
  ASSERT_FALSE(greeting_label()->GetVisible());

  // Close and reopen the Assistant UI.
  CloseAssistantUi();
  ShowAssistantUi();

  EXPECT_TRUE(greeting_label()->GetVisible());
}

TEST_F(AssistantPageViewTest,
       ShouldNotShowGreetingLabelWhenOpeningFromSearchResult) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchResult);

  EXPECT_FALSE(greeting_label()->GetVisible());
}

TEST_F(AssistantPageViewTest, ShouldFocusMicViewWhenPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();

  ClickOnAndWait(voice_input_toggle());

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTest,
       ShouldStartVoiceInteractionWhenPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();

  ClickOnAndWait(voice_input_toggle());

  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kVoice);
}

TEST_F(AssistantPageViewTest,
       ShouldStopVoiceInteractionWhenPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kVoice);

  ClickOnAndWait(keyboard_input_toggle());

  EXPECT_FALSE(current_interaction().has_value());
}

TEST_F(AssistantPageViewTest,
       ShouldFocusTextFieldWhenPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();

  ClickOnAndWait(keyboard_input_toggle());

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, RememberAndShowHistory) {
  ShowAssistantUiInTextMode();
  EXPECT_HAS_FOCUS(input_text_field());

  MockAssistantInteractionWithQueryAndResponse("query 1", "response 1");
  MockAssistantInteractionWithQueryAndResponse("query 2", "response 2");

  EXPECT_HAS_FOCUS(input_text_field());

  EXPECT_TRUE(input_text_field()->GetText().empty());

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_UP, /*flags=*/0);
  EXPECT_EQ(input_text_field()->GetText(), base::UTF8ToUTF16("query 2"));

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_UP, /*flags=*/0);
  EXPECT_EQ(input_text_field()->GetText(), base::UTF8ToUTF16("query 1"));

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_UP, /*flags=*/0);
  EXPECT_EQ(input_text_field()->GetText(), base::UTF8ToUTF16("query 1"));

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_DOWN, /*flags=*/0);
  EXPECT_EQ(input_text_field()->GetText(), base::UTF8ToUTF16("query 2"));

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_DOWN, /*flags=*/0);
  EXPECT_TRUE(input_text_field()->GetText().empty());
}

// Tests the |AssistantPageView| with tablet mode enabled.
class AssistantPageViewTabletModeTest : public AssistantAshTestBase {
 public:
  AssistantPageViewTabletModeTest() = default;

  void SetUp() override {
    AssistantAshTestBase::SetUp();
    SetTabletMode(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantPageViewTabletModeTest);
};

TEST_F(AssistantPageViewTabletModeTest,
       ShouldFocusMicWhenOpeningWithLongPressLauncher) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldFocusMicWhenOpeningWithHotword) {
  ShowAssistantUi(AssistantEntryPoint::kHotword);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldFocusTextFieldAfterSendingQuery) {
  ShowAssistantUi();

  SendQueryThroughTextField("The query");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldHideKeyboardAfterSendingQuery) {
  ShowAssistantUi();
  ShowKeyboard();

  SendQueryThroughTextField("The query");

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldShowKeyboardAfterTouchingInputTextbox) {
  ShowAssistantUi();
  EXPECT_FALSE(IsKeyboardShowing());

  TapOnAndWait(input_text_field());

  EXPECT_TRUE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissWhenTappingOutsideWithinAppListView) {
  ShowAssistantUi();
  EXPECT_TRUE(IsVisible());

  // Tapping position should be outside the Assistant UI but still inside the
  // bounds of |AppList| window.
  gfx::Point origin = page_view()->origin();
  gfx::Point point_outside = gfx::Point(origin.x() - 10, origin.y());
  EXPECT_TRUE(window()->bounds().Contains(point_outside));
  EXPECT_FALSE(page_view()->bounds().Contains(point_outside));

  // Tapping outside on the empty region of |AppListView| should dismiss the
  // Assistant UI.
  GestureEventForTest tap_outside(point_outside,
                                  ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  app_list_view()->OnGestureEvent(&tap_outside);
  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissIfLostFocusWhenOtherAppWindowOpens) {
  ShowAssistantUi();
  EXPECT_TRUE(IsVisible());

  // Creates a new window to steal the focus should dismiss the Assistant UI.
  SwitchToNewAppWindow();
  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldNotDismissWhenTappingInside) {
  ShowAssistantUi();
  EXPECT_TRUE(IsVisible());

  // Tapping position should be inside the Assistant UI.
  gfx::Point origin = page_view()->origin();
  gfx::Point point_inside = gfx::Point(origin.x() + 10, origin.y() + 10);
  EXPECT_TRUE(page_view()->bounds().Contains(point_inside));

  // Tapping inside should not dismiss the Assistant UI.
  GestureEventForTest tap_inside(point_inside,
                                 ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  page_view()->OnGestureEvent(&tap_inside);

  EXPECT_TRUE(IsVisible());
}

}  // namespace ash
