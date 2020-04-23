// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
class AssistantFooterView;
class AssistantProgressIndicator;
class AssistantQueryView;
class AssistantViewDelegate;
class UiElementContainerView;

// AppListAssistantMainStage is the child of AssistantMainView responsible for
// displaying the Assistant interaction to the user. This includes visual
// affordances for the query, response, as well as suggestions.
class APP_LIST_EXPORT AppListAssistantMainStage
    : public views::View,
      public views::ViewObserver,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AppListAssistantMainStage(AssistantViewDelegate* delegate);
  ~AppListAssistantMainStage() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryCleared(bool due_to_commit) override;
  void OnResponseChanged(
      const scoped_refptr<AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  std::unique_ptr<views::View> CreateContentLayoutContainer();
  std::unique_ptr<views::Label> InitGreetingLabel();
  std::unique_ptr<views::View> CreateMainContentLayoutContainer();
  std::unique_ptr<views::View> CreateDividerLayoutContainer();
  std::unique_ptr<views::View> CreateFooterLayoutContainer();

  void AnimateInGreetingLabel();
  void AnimateInFooter();

  void MaybeHideGreetingLabel();

  AssistantViewDelegate* const delegate_;  // Owned by Shell.

  // Owned by view hierarchy.
  AssistantProgressIndicator* progress_indicator_;
  views::View* horizontal_separator_;
  AssistantQueryView* query_view_;
  UiElementContainerView* ui_element_container_;
  views::Label* greeting_label_;
  AssistantFooterView* footer_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<AssistantInteractionController,
                 AssistantInteractionModelObserver,
                 &AssistantInteractionController::AddModelObserver,
                 &AssistantInteractionController::RemoveModelObserver>
      assistant_interaction_model_observer_{this};

  ScopedObserver<AssistantUiController,
                 AssistantUiModelObserver,
                 &AssistantUiController::AddModelObserver,
                 &AssistantUiController::RemoveModelObserver>
      assistant_ui_model_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListAssistantMainStage);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
