// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class TestAssistantService : public chromeos::assistant::mojom::Assistant {
 public:
  TestAssistantService();
  ~TestAssistantService() override;

  chromeos::assistant::mojom::AssistantPtr CreateInterfacePtrAndBind();

  // mojom::Assistant overrides:
  void StartCachedScreenContextInteraction() override {}
  void StartEditReminderInteraction(const std::string& client_id) override {}
  void StartMetalayerInteraction(const gfx::Rect& region) override {}
  void StartTextInteraction(const std::string& query, bool allow_tts) override {
  }
  void StartVoiceInteraction() override {}
  void StartWarmerWelcomeInteraction(int num_warmer_welcome_triggered,
                                     bool allow_tts) override {}
  void StopActiveInteraction(bool cancel_conversation) override {}
  void AddAssistantInteractionSubscriber(
      mojo::PendingRemote<
          chromeos::assistant::mojom::AssistantInteractionSubscriber>
          subscriber) override {}
  void RetrieveNotification(
      chromeos::assistant::mojom::AssistantNotificationPtr notification,
      int action_index) override {}
  void DismissNotification(chromeos::assistant::mojom::AssistantNotificationPtr
                               notification) override {}
  void CacheScreenContext(CacheScreenContextCallback callback) override;
  void ClearScreenContextCache() override {}
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override {}
  void SendAssistantFeedback(
      chromeos::assistant::mojom::AssistantFeedbackPtr feedback) override {}
  void StopAlarmTimerRinging() override {}
  void CreateTimer(base::TimeDelta duration) override {}

 private:
  mojo::Binding<chromeos::assistant::mojom::Assistant> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAssistantService);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
