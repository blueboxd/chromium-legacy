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
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

AssistantAshTestBase::AssistantAshTestBase() = default;

AssistantAshTestBase::~AssistantAshTestBase() = default;

void AssistantAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      app_list_features::kEnableEmbeddedAssistantUI);

  AshTestBase::SetUp();

  // Enable Assistant in settings.
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantEnabled, true);

  // Cache controller.
  controller_ = Shell::Get()->assistant_controller();
  DCHECK(controller_);

  // At this point our Assistant service is ready for use.
  // Indicate this by changing status from NOT_READY to STOPPED.
  AssistantState::Get()->NotifyStatusChanged(mojom::AssistantState::READY);

  DisableAnimations();
}

void AssistantAshTestBase::TearDown() {
  AshTestBase::TearDown();
  ReenableAnimations();
}

const app_list::AssistantMainView* AssistantAshTestBase::main_view() const {
  return page_view()->GetMainViewForTest();
}

const app_list::AssistantPageView* AssistantAshTestBase::page_view() const {
  const int index = contents_view()->GetPageIndexForState(
      ash::AppListState::kStateEmbeddedAssistant);
  return static_cast<app_list::AssistantPageView*>(
      contents_view()->GetPageView(index));
}

void AssistantAshTestBase::MockAssistantInteractionWithResponse(
    const std::string& response_text) {
  const std::string query = std::string("input text");

  // |controller_| will blackhole any server response if it hasn't sent
  // a request first, so we need to start by sending a request.
  controller_->interaction_controller()->OnDialogPlateContentsCommitted(query);
  // Then the server can start an interaction and return the response.
  controller_->interaction_controller()->OnInteractionStarted(
      chromeos::assistant::mojom::AssistantInteractionMetadata::New(
          /*type=*/chromeos::assistant::mojom::AssistantInteractionType::kText,
          /*query=*/query));
  controller_->interaction_controller()->OnTextResponse(response_text);
  controller_->interaction_controller()->OnInteractionFinished(
      AssistantInteractionController::AssistantInteractionResolution::kNormal);
}

void AssistantAshTestBase::ShowAssistantUi(AssistantEntryPoint entry_point) {
  controller_->ui_controller()->ShowUi(entry_point);
}

const app_list::ContentsView* AssistantAshTestBase::contents_view() const {
  auto* app_list_view =
      Shell::Get()->app_list_controller()->presenter()->GetView();

  DCHECK(app_list_view) << "AppListView has not been initialized yet. "
                           "Be sure to call |ShowAssistantUI| first";

  return app_list_view->app_list_main_view()->contents_view();
}

void AssistantAshTestBase::DisableAnimations() {
  app_list::AppListView::SetShortAnimationForTesting(true);

  // The |AssistantProcessIndicator| uses a cyclic animation,
  // which will go in an infinite loop if the animation has ZERO_DURATION.
  // So we use a NON_ZERO_DURATION instead.
  scoped_animation_duration_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
}

void AssistantAshTestBase::ReenableAnimations() {
  scoped_animation_duration_ = nullptr;
  app_list::AppListView::SetShortAnimationForTesting(false);
}

}  // namespace ash
