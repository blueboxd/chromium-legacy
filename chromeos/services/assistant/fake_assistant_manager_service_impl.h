// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/fake_assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace chromeos {
namespace assistant {

// Stub implementation of AssistantManagerService.  Should return deterministic
// result for testing.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) FakeAssistantManagerServiceImpl
    : public AssistantManagerService {
 public:
  FakeAssistantManagerServiceImpl();
  ~FakeAssistantManagerServiceImpl() override;

  void FinishStart();

  // assistant::AssistantManagerService overrides
  void Start(const base::Optional<std::string>& access_token,
             bool enable_hotword,
             base::OnceClosure callback) override;
  void Stop() override;
  void SetAccessToken(const std::string& access_token) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enabled) override;
  State GetState() const override;
  AssistantSettingsManager* GetAssistantSettingsManager() override;

  // mojom::Assistant overrides:
  void StartCachedScreenContextInteraction() override;
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartMetalayerInteraction(const gfx::Rect& region) override;
  void StartTextInteraction(const std::string& query, bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StartWarmerWelcomeInteraction(int num_warmer_welcome_triggered,
                                     bool allow_tts) override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      mojom::AssistantInteractionSubscriberPtr subscriber) override;
  void RetrieveNotification(mojom::AssistantNotificationPtr notification,
                            int action_index) override;
  void DismissNotification(
      mojom::AssistantNotificationPtr notification) override;
  void CacheScreenContext(CacheScreenContextCallback callback) override;
  void ClearScreenContextCache() override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;
  void SendAssistantFeedback(mojom::AssistantFeedbackPtr feedback) override;
  void StopAlarmTimerRinging() override;
  void CreateTimer(base::TimeDelta duration) override;

 private:
  State state_ = State::STOPPED;
  base::OnceClosure start_callback_;
  FakeAssistantSettingsManagerImpl assistant_settings_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
