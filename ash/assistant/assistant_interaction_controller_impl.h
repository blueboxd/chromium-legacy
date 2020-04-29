// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class AssistantControllerImpl;
class AssistantInteractionModelObserver;
class ProactiveSuggestions;
enum class AssistantButtonId;
enum class AssistantQuerySource;

class AssistantInteractionControllerImpl
    : public AssistantInteractionController,
      public chromeos::assistant::mojom::AssistantInteractionSubscriber,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver,
      public TabletModeObserver,
      public HighlighterController::Observer {
 public:
  using AssistantInteractionMetadata =
      chromeos::assistant::mojom::AssistantInteractionMetadata;
  using AssistantInteractionMetadataPtr =
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr;
  using AssistantInteractionResolution =
      chromeos::assistant::mojom::AssistantInteractionResolution;
  using AssistantInteractionType =
      chromeos::assistant::mojom::AssistantInteractionType;
  using AssistantQuerySource = chromeos::assistant::mojom::AssistantQuerySource;
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;
  using AssistantSuggestionType =
      chromeos::assistant::mojom::AssistantSuggestionType;

  explicit AssistantInteractionControllerImpl(
      AssistantControllerImpl* assistant_controller);
  ~AssistantInteractionControllerImpl() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // AssistantInteractionController:
  const AssistantInteractionModel* GetModel() const override;
  void AddModelObserver(AssistantInteractionModelObserver*) override;
  void RemoveModelObserver(AssistantInteractionModelObserver*) override;
  void StartTextInteraction(const std::string& text,
                            bool allow_tts,
                            AssistantQuerySource query_source) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnMicStateChanged(MicState mic_state) override;
  void OnCommittedQueryChanged(const AssistantQuery& assistant_query) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // HighlighterController::Observer:
  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override;

  // chromeos::assistant::mojom::AssistantInteractionSubscriber:
  void OnInteractionStarted(AssistantInteractionMetadataPtr metadata) override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;
  void OnSuggestionsResponse(
      std::vector<AssistantSuggestionPtr> response) override;
  void OnTextResponse(const std::string& response) override;
  void OnOpenUrlResponse(const GURL& url, bool in_background) override;
  void OnOpenAppResponse(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                         OnOpenAppResponseCallback callback) override;
  void OnSpeechRecognitionStarted() override;
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;
  void OnSpeechRecognitionEndOfUtterance() override;
  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;
  void OnSpeechLevelUpdated(float speech_level) override;
  void OnTtsStarted(bool due_to_error) override;
  void OnWaitStarted() override;

  // AssistantViewDelegateObserver:
  void OnDialogPlateButtonPressed(AssistantButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;
  void OnSuggestionChipPressed(const AssistantSuggestion* suggestion) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  void OnTabletModeChanged();

  bool HasUnprocessedPendingResponse();
  bool HasActiveInteraction() const;

  void OnProcessPendingResponse();
  void OnPendingResponseProcessed(bool is_completed);

  void OnUiVisible(AssistantEntryPoint entry_point);
  bool ShouldAttemptWarmerWelcome(AssistantEntryPoint entry_point) const;
  void AttemptWarmerWelcome();

  void StartProactiveSuggestionsInteraction(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions);
  void StartScreenContextInteraction(bool include_assistant_structure,
                                     const gfx::Rect& region,
                                     AssistantQuerySource query_source);

  void StartVoiceInteraction();
  void StopActiveInteraction(bool cancel_conversation);

  InputModality GetDefaultInputModality() const;
  AssistantResponse* GetResponseForActiveInteraction();
  AssistantVisibility GetVisibility() const;
  bool IsVisible() const;

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  mojo::Receiver<chromeos::assistant::mojom::AssistantInteractionSubscriber>
      assistant_interaction_subscriber_receiver_{this};

  AssistantInteractionModel model_;

  // The number of times the Assistant UI has been shown (since the device
  // booted).
  // Might overflow so do not use for super critical things.
  int number_of_times_shown_ = 0;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<HighlighterController, HighlighterController::Observer>
      highlighter_controller_observer_{this};

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_controller_observer_{this};

  base::WeakPtrFactory<AssistantInteractionControllerImpl>
      screen_context_request_factory_{this};
  base::WeakPtrFactory<AssistantInteractionControllerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionControllerImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_IMPL_H_
