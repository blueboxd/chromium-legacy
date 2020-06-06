// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_interaction_logger.h"

#include <utility>

namespace chromeos {
namespace assistant {

namespace {

std::string ResolutionToString(
    chromeos::assistant::mojom::AssistantInteractionResolution resolution) {
  std::stringstream result;
  result << resolution;
  return result.str();
}

#define LOG_INTERACTION()                      \
  VLOG(AssistantInteractionLogger::kVLogLevel) \
      << "Assistant: " << __FUNCTION__ << ": "

}  // namespace

bool AssistantInteractionLogger::IsLoggingEnabled() {
  return VLOG_IS_ON(kVLogLevel);
}

AssistantInteractionLogger::AssistantInteractionLogger() = default;

AssistantInteractionLogger::~AssistantInteractionLogger() = default;

mojo::PendingRemote<mojom::AssistantInteractionSubscriber>
AssistantInteractionLogger::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void AssistantInteractionLogger::OnInteractionStarted(
    chromeos::assistant::mojom::AssistantInteractionMetadataPtr metadata) {
  switch (metadata->type) {
    case mojom::AssistantInteractionType::kText:
      LOG_INTERACTION() << "Text interaction with query '" << metadata->query
                        << "'";
      break;
    case mojom::AssistantInteractionType::kVoice:
      LOG_INTERACTION() << "Voice interaction";
      break;
  }
}

void AssistantInteractionLogger::OnInteractionFinished(
    chromeos::assistant::mojom::AssistantInteractionResolution resolution) {
  LOG_INTERACTION() << "with resolution " << ResolutionToString(resolution);
}

void AssistantInteractionLogger::OnHtmlResponse(const std::string& response,
                                                const std::string& fallback) {
  // Displaying fallback instead of the response as the response is filled with
  // HTML tags and rather large.
  LOG_INTERACTION() << "with fallback '" << fallback << "'";
}

void AssistantInteractionLogger::OnSuggestionsResponse(
    std::vector<chromeos::assistant::mojom::AssistantSuggestionPtr> response) {
  std::stringstream suggestions;
  for (const auto& suggestion : response)
    suggestions << "'" << suggestion->text << "', ";
  LOG_INTERACTION() << "{ " << suggestions.str() << " }";
}

void AssistantInteractionLogger::OnTextResponse(const std::string& response) {
  LOG_INTERACTION() << "'" << response << "'";
}

void AssistantInteractionLogger::OnOpenUrlResponse(const ::GURL& url,
                                                   bool in_background) {
  LOG_INTERACTION() << "with url '" << url.possibly_invalid_spec() << "'";
}

void AssistantInteractionLogger::OnOpenAppResponse(
    chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
    OnOpenAppResponseCallback callback) {
  LOG_INTERACTION() << "with app '" << app_info->package_name << "'";
  std::move(callback).Run(true);
}

void AssistantInteractionLogger::OnSpeechRecognitionStarted() {
  LOG_INTERACTION();
}

void AssistantInteractionLogger::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  // Not logged until we have a use for this (and this might spam the log).
}

void AssistantInteractionLogger::OnSpeechRecognitionEndOfUtterance() {
  LOG_INTERACTION();
}

void AssistantInteractionLogger::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  LOG_INTERACTION() << "with final result '" << final_result << "'";
}

void AssistantInteractionLogger::OnSpeechLevelUpdated(float speech_level) {
  // Not logged until we have a use for this and this might spam the log.
}

void AssistantInteractionLogger::OnTtsStarted(bool due_to_error) {
  LOG_INTERACTION() << (due_to_error ? "not" : "") << "due to error";
}

void AssistantInteractionLogger::OnWaitStarted() {
  LOG_INTERACTION();
}

}  // namespace assistant
}  // namespace chromeos
