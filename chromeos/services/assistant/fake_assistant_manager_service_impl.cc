// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"

#include <utility>

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceImpl::FakeAssistantManagerServiceImpl() = default;

FakeAssistantManagerServiceImpl::~FakeAssistantManagerServiceImpl() = default;

void FakeAssistantManagerServiceImpl::FinishStart() {
  DCHECK(start_callback_);
  state_ = State::RUNNING;
  std::move(start_callback_).Run();
}

void FakeAssistantManagerServiceImpl::Start(
    const base::Optional<std::string>& access_token,
    bool enable_hotword,
    base::OnceClosure callback) {
  state_ = State::STARTED;
  start_callback_ = std::move(callback);
}

void FakeAssistantManagerServiceImpl::Stop() {
  state_ = State::STOPPED;
}

void FakeAssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {}

void FakeAssistantManagerServiceImpl::EnableListening(bool enable) {}

void FakeAssistantManagerServiceImpl::EnableHotword(bool enable) {}

void FakeAssistantManagerServiceImpl::SetArcPlayStoreEnabled(bool enabled) {}

AssistantManagerService::State FakeAssistantManagerServiceImpl::GetState()
    const {
  return state_;
}

AssistantSettingsManager*
FakeAssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return &assistant_settings_manager_;
}

void FakeAssistantManagerServiceImpl::StartCachedScreenContextInteraction() {}

void FakeAssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {}

void FakeAssistantManagerServiceImpl::StartMetalayerInteraction(
    const gfx::Rect& region) {}

void FakeAssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    bool allow_tts) {}

void FakeAssistantManagerServiceImpl::StartVoiceInteraction() {}

void FakeAssistantManagerServiceImpl::StartWarmerWelcomeInteraction(
    int num_warmer_welcome_triggered,
    bool allow_tts) {}

void FakeAssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {}

void FakeAssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojom::AssistantInteractionSubscriberPtr subscriber) {}

void FakeAssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {}

void FakeAssistantManagerServiceImpl::DismissNotification(
    mojom::AssistantNotificationPtr notification) {}

void FakeAssistantManagerServiceImpl::CacheScreenContext(
    CacheScreenContextCallback callback) {
  std::move(callback).Run();
}

void FakeAssistantManagerServiceImpl::ClearScreenContextCache() {}

void FakeAssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

void FakeAssistantManagerServiceImpl::SendAssistantFeedback(
    mojom::AssistantFeedbackPtr feedback) {}

void FakeAssistantManagerServiceImpl::StopAlarmTimerRinging() {}
void FakeAssistantManagerServiceImpl::CreateTimer(base::TimeDelta duration) {}

}  // namespace assistant
}  // namespace chromeos
