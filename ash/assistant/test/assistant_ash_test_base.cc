// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/assistant_ash_test_base.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/assistant/assistant_page_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionType;

gfx::Point GetPointInside(const views::View* view) {
  return view->GetBoundsInScreen().CenterPoint();
}

bool CanProcessEvents(const views::View* view) {
  const views::View* ancestor = view;
  while (ancestor != nullptr) {
    if (!ancestor->CanProcessEventsWithinSubtree())
      return false;
    ancestor = ancestor->parent();
  }
  return true;
}

views::View* GetDescendantViewWithNameOrNull(views::View* parent,
                                             const std::string& name) {
  for (views::View* child : parent->children()) {
    if (child->GetClassName() == name)
      return child;

    views::View* descendant = GetDescendantViewWithNameOrNull(child, name);
    if (descendant)
      return descendant;
  }
  return nullptr;
}

// Recursively search for a descendant view with the given name.
// Asserts if no such view exists.
views::View* GetDescendantViewWithName(views::View* parent,
                                       const std::string& name) {
  views::View* descendant_maybe = GetDescendantViewWithNameOrNull(parent, name);
  if (descendant_maybe == nullptr) {
    ADD_FAILURE() << "View " << parent->GetClassName()
                  << " has no descendant with name '" << name << "'.";
  }
  return descendant_maybe;
}

}  // namespace

AssistantAshTestBase::AssistantAshTestBase() = default;

AssistantAshTestBase::~AssistantAshTestBase() = default;

void AssistantAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      app_list_features::kEnableEmbeddedAssistantUI);

  // Enable virtual keyboard.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      keyboard::switches::kEnableVirtualKeyboard);

  AshTestBase::SetUp();

  // Make the display big enough to hold the app list.
  UpdateDisplay("1024x768");

  // Enable Assistant in settings.
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantEnabled, true);

  // Cache controller.
  controller_ = Shell::Get()->assistant_controller();
  DCHECK(controller_);

  // At this point our Assistant service is ready for use.
  // Indicate this by changing status from NOT_READY to READY.
  AssistantState::Get()->NotifyStatusChanged(mojom::AssistantState::READY);

  DisableAnimations();

  // Wait for virtual keyboard to load.
  SetTouchKeyboardEnabled(true);
}

void AssistantAshTestBase::TearDown() {
  windows_.clear();
  SetTouchKeyboardEnabled(false);
  AshTestBase::TearDown();
  scoped_feature_list_.Reset();
  ReenableAnimations();
}

void AssistantAshTestBase::ShowAssistantUi(AssistantEntryPoint entry_point) {
  if (entry_point == AssistantEntryPoint::kHotword) {
    // If the Assistant is triggered via Hotword, the interaction is triggered
    // by the Assistant service.
    assistant_service()->StartVoiceInteraction();
  } else {
    // Otherwise, the interaction is triggered by a call to |ShowUi|.
    controller_->ui_controller()->ShowUi(entry_point);
  }
  // Send all mojom messages to/from the assistant service.
  base::RunLoop().RunUntilIdle();
}

void AssistantAshTestBase::CloseAssistantUi(AssistantExitPoint exit_point) {
  controller_->ui_controller()->CloseUi(exit_point);
}

void AssistantAshTestBase::CloseLauncher() {
  ash::Shell::Get()->app_list_controller()->ViewClosing();
}

void AssistantAshTestBase::SetTabletMode(bool enable) {
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
}

void AssistantAshTestBase::SetPreferVoice(bool prefer_voice) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen, prefer_voice);
}

AssistantMainView* AssistantAshTestBase::main_view() {
  return page_view()->GetMainViewForTest();
}

AssistantPageView* AssistantAshTestBase::page_view() {
  const int index = contents_view()->GetPageIndexForState(
      AppListState::kStateEmbeddedAssistant);
  return static_cast<AssistantPageView*>(contents_view()->GetPageView(index));
}

void AssistantAshTestBase::MockAssistantInteractionWithResponse(
    const std::string& response_text) {
  const std::string query = std::string("input text");

  SendQueryThroughTextField(query);
  assistant_service()->SetInteractionResponse(
      InteractionResponse()
          .AddTextResponse(response_text)
          .AddResolution(InteractionResponse::Resolution::kNormal)
          .Clone());

  base::RunLoop().RunUntilIdle();
}

void AssistantAshTestBase::SendQueryThroughTextField(const std::string& query) {
  if (!input_text_field()->HasFocus()) {
    ADD_FAILURE()
        << "The TextField should be focussed before we can send a query";
  }

  input_text_field()->SetText(base::ASCIIToUTF16(query));
  // Send <return> to commit the query.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN,
                                /*flags=*/ui::EF_NONE);
}

void AssistantAshTestBase::TapOnTextField() {
  if (!CanProcessEvents(input_text_field()))
    ADD_FAILURE() << "TextField can not process tap events";

  GetEventGenerator()->GestureTapAt(GetPointInside(input_text_field()));
}

aura::Window* AssistantAshTestBase::SwitchToNewAppWindow() {
  windows_.push_back(CreateAppWindow());

  aura::Window* window = windows_.back().get();
  window->SetName("<app-window>");
  return window;
}

aura::Window* AssistantAshTestBase::window() {
  return main_view()->GetWidget()->GetNativeWindow();
}

views::Textfield* AssistantAshTestBase::input_text_field() {
  views::View* result = GetDescendantViewWithName(main_view(), "Textfield");
  return static_cast<views::Textfield*>(result);
}

views::View* AssistantAshTestBase::mic_view() {
  return GetDescendantViewWithName(main_view(), "MicView");
}

void AssistantAshTestBase::ShowKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/false);
}

bool AssistantAshTestBase::IsKeyboardShowing() const {
  return keyboard::IsKeyboardShowing();
}

ContentsView* AssistantAshTestBase::contents_view() {
  auto* app_list_view =
      Shell::Get()->app_list_controller()->presenter()->GetView();

  DCHECK(app_list_view) << "AppListView has not been initialized yet. "
                           "Be sure to call |ShowAssistantUI| first";

  return app_list_view->app_list_main_view()->contents_view();
}

AssistantInteractionController* AssistantAshTestBase::interaction_controller() {
  return controller_->interaction_controller();
}

TestAssistantService* AssistantAshTestBase::assistant_service() {
  return ash_test_helper()->test_assistant_service();
}

void AssistantAshTestBase::DisableAnimations() {
  AppListView::SetShortAnimationForTesting(true);

  scoped_animation_duration_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

void AssistantAshTestBase::ReenableAnimations() {
  scoped_animation_duration_ = nullptr;
  AppListView::SetShortAnimationForTesting(false);
}

}  // namespace ash
