// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class Assistant;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

class ASH_EXPORT AssistantUiControllerImpl
    : public AssistantUiController,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public HighlighterController::Observer,
      public OverviewObserver {
 public:
  AssistantUiControllerImpl();
  ~AssistantUiControllerImpl() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // AssistantUiController:
  const AssistantUiModel* GetModel() const override;
  void AddModelObserver(AssistantUiModelObserver* observer) override;
  void RemoveModelObserver(AssistantUiModelObserver* observer) override;
  void ShowUi(AssistantEntryPoint entry_point) override;
  void CloseUi(AssistantExitPoint exit_point) override;
  void ToggleUi(base::Optional<AssistantEntryPoint> entry_point,
                base::Optional<AssistantExitPoint> exit_point) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnMicStateChanged(MicState mic_state) override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnOpeningUrl(const GURL& url,
                    bool in_background,
                    bool from_server) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;

 private:
  // Updates UI mode to |ui_mode| if specified. Otherwise UI mode is updated on
  // the basis of interaction/widget visibility state. If |due_to_interaction|
  // is true, the UI mode changed because of an Assistant interaction.
  void UpdateUiMode(base::Optional<AssistantUiMode> ui_mode = base::nullopt,
                    bool due_to_interaction = false);

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantUiModel model_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<HighlighterController, HighlighterController::Observer>
      highlighter_controller_observer_{this};

  ScopedObserver<OverviewController, OverviewObserver>
      overview_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantUiControllerImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_
