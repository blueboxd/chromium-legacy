// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_

#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace assistant {

class MockAssistantInteractionSubscriber
    : public mojom::AssistantInteractionSubscriber {
 public:
  MockAssistantInteractionSubscriber();
  MockAssistantInteractionSubscriber(
      const MockAssistantInteractionSubscriber&) = delete;
  MockAssistantInteractionSubscriber& operator=(
      const MockAssistantInteractionSubscriber&) = delete;
  ~MockAssistantInteractionSubscriber() override;

  // mojom::AssistantInteractionSubscriber:
  MOCK_METHOD(void,
              OnInteractionStarted,
              (mojom::AssistantInteractionMetadataPtr),
              (override));
  MOCK_METHOD(void,
              OnInteractionFinished,
              (mojom::AssistantInteractionResolution),
              (override));
  MOCK_METHOD(void,
              OnHtmlResponse,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsResponse,
              (std::vector<mojom::AssistantSuggestionPtr>),
              (override));
  MOCK_METHOD(void, OnTextResponse, (const std::string&), (override));
  MOCK_METHOD(void, OnOpenUrlResponse, (const ::GURL&, bool), (override));
  MOCK_METHOD(void,
              OnOpenAppResponse,
              (mojom::AndroidAppInfoPtr, OnOpenAppResponseCallback),
              (override));
  MOCK_METHOD(void, OnSpeechRecognitionStarted, (), (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionIntermediateResult,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, OnSpeechRecognitionEndOfUtterance, (), (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionFinalResult,
              (const std::string&),
              (override));
  MOCK_METHOD(void, OnSpeechLevelUpdated, (float), (override));
  MOCK_METHOD(void, OnTtsStarted, (bool), (override));
  MOCK_METHOD(void, OnWaitStarted, (), (override));
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_
