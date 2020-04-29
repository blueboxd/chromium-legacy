// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEFAULT_ASSISTANT_INTERACTION_SUBSCRIBER_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEFAULT_ASSISTANT_INTERACTION_SUBSCRIBER_H_

#include <string>
#include <vector>

#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// Implementation of |AssistantInteractionSubscriber| that has a default, empty
// implementation for each virtual method.
// This reduces the clutter if a derived class only cares about a few of the
// methods.
// It also contains a |mojo::Receiver| as every derived class needs this anyway.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
    DefaultAssistantInteractionSubscriber
    : public mojom::AssistantInteractionSubscriber {
 public:
  DefaultAssistantInteractionSubscriber();
  ~DefaultAssistantInteractionSubscriber() override;

  mojo::PendingRemote<AssistantInteractionSubscriber>
  BindNewPipeAndPassRemote();

  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr) override {}
  void OnInteractionFinished(
      chromeos::assistant::mojom::AssistantInteractionResolution) override {}
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {}
  void OnSuggestionsResponse(
      std::vector<chromeos::assistant::mojom::AssistantSuggestionPtr> response)
      override {}
  void OnTextResponse(const std::string& response) override {}
  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {}
  void OnOpenAppResponse(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                         OnOpenAppResponseCallback callback) override {}
  void OnSpeechRecognitionStarted() override {}
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override {}
  void OnSpeechRecognitionEndOfUtterance() override {}
  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {}
  void OnSpeechLevelUpdated(float speech_level) override {}
  void OnTtsStarted(bool due_to_error) override {}
  void OnWaitStarted() override {}

 private:
  mojo::Receiver<AssistantInteractionSubscriber> receiver_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_DEFAULT_ASSISTANT_INTERACTION_SUBSCRIBER_H_
