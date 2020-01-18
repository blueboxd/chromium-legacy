// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-shared.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

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

// Ensures that the given view does not have the focus. If it does, this will
// print a nice error message.
#define EXPECT_NOT_HAS_FOCUS(expected_)                  \
  ({                                                     \
    EXPECT_FALSE(expected_->HasFocus())                  \
        << "'" << expected_->GetClassName()              \
        << "' should not have the focus (but it does)."; \
  })

views::View* AddTextfield(views::Widget* widget) {
  auto* result = widget->GetContentsView()->AddChildView(
      std::make_unique<views::Textfield>());
  // Give the text field a non-zero size, otherwise things like tapping on it
  // will fail.
  result->SetSize(gfx::Size(20, 10));
  return result;
}

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

// |ViewObserver| that simply remembers whether the given view was drawn
// on the screen at least once during the lifetime of this observer.
// Note this checks |IsDrawn| and not |GetVisible| because visibility is a
// local property which does not take any of the ancestors into account, and
// we do not care if the |observed_view| is marked as visible but its parent
// is not visible.
class VisibilityObserver : public views::ViewObserver {
 public:
  explicit VisibilityObserver(views::View* observed_view)
      : observed_view_(observed_view) {
    observed_view_->AddObserver(this);

    UpdateWasDrawn();
  }
  ~VisibilityObserver() override { observed_view_->RemoveObserver(this); }

  void OnViewVisibilityChanged(views::View* view_or_ancestor,
                               views::View* starting_view) override {
    UpdateWasDrawn();
  }

  bool was_drawn() const { return was_drawn_; }

 private:
  void UpdateWasDrawn() {
    if (observed_view_->IsDrawn())
      was_drawn_ = true;
  }

  views::View* const observed_view_;
  bool was_drawn_ = false;
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
    EXPECT_TRUE(IsVisible());
  }

  void ShowAssistantUiInVoiceMode() {
    ShowAssistantUi(AssistantEntryPoint::kHotword);
    EXPECT_TRUE(IsVisible());
  }

  // Returns a point in the AppList, but outside the Assistant UI.
  gfx::Point GetPointInAppListOutsideAssistantUi() {
    gfx::Point result = GetPointOutside(page_view());

    // Sanity check
    EXPECT_TRUE(app_list_view()->bounds().Contains(result));
    EXPECT_FALSE(page_view()->bounds().Contains(result));

    return result;
  }

  gfx::Point GetPointOutside(const views::View* view) {
    return gfx::Point(view->origin().x() - 10, view->origin().y() - 10);
  }

  gfx::Point GetPointInside(const views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantPageViewTest);
};

}  // namespace

TEST_F(AssistantPageViewTest, ShouldStartAtMinimumHeight) {
  ShowAssistantUi();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest,
       ShouldRemainAtMinimumHeightWhenDisplayingOneLiner) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse("Short one-liner");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kMinHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWithMultilineText) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse(
      "This\ntext\nhas\na\nlot\nof\nlinebreaks.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kMaxHeightEmbeddedDip, main_view()->size().height());
}

TEST_F(AssistantPageViewTest, ShouldGetBiggerWhenWrappingTextLine) {
  ShowAssistantUi();

  MockAssistantInteractionWithResponse(
      "This is a very long text without any linebreaks. "
      "This will wrap, and should cause the Assistant view to get bigger. "
      "If it doesn't, this looks really bad. This is what caused b/134963994.");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kMaxHeightEmbeddedDip, main_view()->size().height());
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

TEST_F(AssistantPageViewTest, ShouldDismissGreetingLabelAfterQuery) {
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

TEST_F(AssistantPageViewTest, ShouldNotClearQueryWhenSwitchingToTabletMode) {
  const base::string16 query_text = base::UTF8ToUTF16("unsubmitted query");
  ShowAssistantUiInTextMode();
  input_text_field()->SetText(query_text);

  SetTabletMode(true);

  EXPECT_HAS_FOCUS(input_text_field());
  EXPECT_EQ(query_text, input_text_field()->GetText());
}

// Tests the |AssistantPageView| with tablet mode enabled.
class AssistantPageViewTabletModeTest : public AssistantPageViewTest {
 public:
  AssistantPageViewTabletModeTest() = default;

  void SetUp() override {
    AssistantPageViewTest::SetUp();
    SetTabletMode(true);
  }

  // Ensures all views are created by showing and hiding the UI.
  void CreateAllViews() {
    ShowAssistantUi();
    CloseAssistantUi();
  }

  void ShowAssistantUiInTextMode() {
    // Note we launch in voice mode and switch to text because that was the
    // easiest way I could get this working (we can't simply use |kUnspecified|
    // as that puts us in voice mode.
    ShowAssistantUiInVoiceMode();
    TapOnAndWait(keyboard_input_toggle());
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
  ShowAssistantUiInTextMode();

  SendQueryThroughTextField("The query");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardAfterSendingQuery) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  SendQueryThroughTextField("The query");

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotShowKeyboardWhenOpeningLauncherButNotAssistantUi) {
  OpenLauncher();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldShowKeyboardAfterTouchingInputTextbox) {
  ShowAssistantUiInTextMode();
  DismissKeyboard();
  EXPECT_FALSE(IsKeyboardShowing());

  TapOnAndWait(input_text_field());

  EXPECT_TRUE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldFocusTextFieldAfterPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_NOT_HAS_FOCUS(input_text_field());

  TapOnAndWait(keyboard_input_toggle());

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldShowKeyboardAfterPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_FALSE(IsKeyboardShowing());

  TapOnAndWait(keyboard_input_toggle());

  EXPECT_TRUE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardAfterPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapOnAndWait(voice_input_toggle());

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardWhenOpeningUiInVoiceMode) {
  // Start by focussing a text field so the system has a reason to show the
  // keyboard.
  views::Widget* widget = SwitchToNewWidget();
  auto* textfield = AddTextfield(widget);
  TapOnAndWait(textfield);
  ASSERT_TRUE(IsKeyboardShowing());

  ShowAssistantUiInVoiceMode();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissAssistantUiIfLostFocusWhenOtherAppWindowOpens) {
  ShowAssistantUi();

  // Create a new window to steal the focus which should dismiss the Assistant
  // UI.
  SwitchToNewAppWindow();

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotShowKeyboardWhenClosingAssistantUI) {
  // Note: This checks for a bug where the closing sequence of the UI switches
  // the input mode to text which then shows the keyboard.
  ShowAssistantUiInVoiceMode();

  CloseAssistantUi();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardWhenClosingAssistantUI) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  // Enable the animations because the fact that the switch-modality animation
  // runs changes the timing enough that it triggers a potential bug where the
  // keyboard remains visible after the UI is closed.
  ui::ScopedAnimationDurationScaleMode enable_animations{
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION};

  CloseAssistantUi();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissAssistantUiWhenTappingAppListView) {
  ShowAssistantUiInVoiceMode();

  TapAndWait(GetPointInAppListOutsideAssistantUi());

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardButNotAssistantUiWhenTappingAppListView) {
  // Note: This is consistent with how the app list search box works;
  // the first tap will dismiss the keyboard but not change the state of the
  // search box.
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapAndWait(GetPointInAppListOutsideAssistantUi());

  EXPECT_FALSE(IsKeyboardShowing());
  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotDismissKeyboardWhenTappingInsideAssistantUi) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapAndWait(GetPointInside(page_view()));

  EXPECT_TRUE(IsKeyboardShowing());
  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotDismissAssistantUiWhenTappingInsideAssistantUi) {
  ShowAssistantUi();

  TapAndWait(GetPointInside(page_view()));

  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotFlickerTextInputWhenOpeningAssistantUI) {
  // If the text input field is visible at any time when opening the Assistant
  // UI, it will cause an unwanted visible animation (of the voice input
  // animating in).

  CreateAllViews();
  VisibilityObserver text_field_observer(input_text_field());

  ShowAssistantUiInVoiceMode();

  EXPECT_FALSE(text_field_observer.was_drawn());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotFlickerTextInputWhenClosingAssistantUI) {
  // Same as above but for the closing animation.
  ShowAssistantUiInVoiceMode();

  VisibilityObserver text_field_observer(input_text_field());

  CloseAssistantUi();

  EXPECT_FALSE(text_field_observer.was_drawn());
}

}  // namespace ash
