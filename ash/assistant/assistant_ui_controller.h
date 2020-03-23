// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class Assistant;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

class AssistantController;

class ASH_EXPORT AssistantUiController
    : public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public HighlighterController::Observer,
      public OverviewObserver {
 public:
  explicit AssistantUiController(AssistantController* assistant_controller);
  ~AssistantUiController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns the underlying model.
  const AssistantUiModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantUiModelObserver* observer);
  void RemoveModelObserver(AssistantUiModelObserver* observer);

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

  void ShowUi(AssistantEntryPoint entry_point);
  void CloseUi(AssistantExitPoint exit_point);
  void ToggleUi(base::Optional<AssistantEntryPoint> entry_point,
                base::Optional<AssistantExitPoint> exit_point);

 private:
  // Updates UI mode to |ui_mode| if specified. Otherwise UI mode is updated on
  // the basis of interaction/widget visibility state. If |due_to_interaction|
  // is true, the UI mode changed because of an Assistant interaction.
  void UpdateUiMode(base::Optional<AssistantUiMode> ui_mode = base::nullopt,
                    bool due_to_interaction = false);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantUiModel model_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
