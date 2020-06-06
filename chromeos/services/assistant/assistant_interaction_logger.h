// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_

#include <string>
#include <vector>

#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// A subscriber that will log all Assistant interactions.
// The interactions will be logged using
//     VLOG(AssistantInteractionLogger::kVLogLevel)
class AssistantInteractionLogger
    : public mojom::AssistantInteractionSubscriber {
 public:
  // VLog level used for logging interactions.
  constexpr static const int kVLogLevel = 1;

  // Returns if the current logging level is high enough so that the traces
  // will be printed. If not, there is no point in creating this class.
  static bool IsLoggingEnabled();

  AssistantInteractionLogger();
  AssistantInteractionLogger(AssistantInteractionLogger&) = delete;
  AssistantInteractionLogger& operator=(AssistantInteractionLogger&) = delete;
  ~AssistantInteractionLogger() override;

  mojo::PendingRemote<mojom::AssistantInteractionSubscriber>
  BindNewPipeAndPassRemote();

  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr metadata)
      override;

  void OnInteractionFinished(
      chromeos::assistant::mojom::AssistantInteractionResolution resolution)
      override;

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;

  void OnSuggestionsResponse(
      std::vector<chromeos::assistant::mojom::AssistantSuggestionPtr> response)
      override;

  void OnTextResponse(const std::string& response) override;

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override;

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

 private:
  mojo::Receiver<AssistantInteractionSubscriber> receiver_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_
