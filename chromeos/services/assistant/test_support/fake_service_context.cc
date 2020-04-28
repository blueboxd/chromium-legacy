// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/test_support/fake_service_context.h"

namespace chromeos {
namespace assistant {

/*static*/
constexpr const char* FakeServiceContext::kGaiaId;

FakeServiceContext::FakeServiceContext() = default;

FakeServiceContext::~FakeServiceContext() = default;

FakeServiceContext& FakeServiceContext::set_assistant_alarm_timer_controller(
    ash::mojom::AssistantAlarmTimerController* value) {
  assistant_alarm_timer_controller_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_main_task_runner(
    scoped_refptr<base::SingleThreadTaskRunner> value) {
  main_task_runner_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_power_manager_client(
    PowerManagerClient* value) {
  power_manager_client_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_primary_account_gaia_id(
    std::string value) {
  gaia_id_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_assistant_state(
    ash::AssistantStateBase* value) {
  assistant_state_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_assistant_notification_controller(
    ash::mojom::AssistantNotificationController* value) {
  assistant_notification_controller_ = value;
  return *this;
}

ash::mojom::AssistantAlarmTimerController*
FakeServiceContext::assistant_alarm_timer_controller() {
  DCHECK(assistant_alarm_timer_controller_ != nullptr);
  return assistant_alarm_timer_controller_;
}

ash::AssistantController* FakeServiceContext::assistant_controller() {
  NOTIMPLEMENTED();
  return nullptr;
}

ash::mojom::AssistantNotificationController*
FakeServiceContext::assistant_notification_controller() {
  DCHECK(assistant_notification_controller_ != nullptr);
  return assistant_notification_controller_;
}

ash::mojom::AssistantScreenContextController*
FakeServiceContext::assistant_screen_context_controller() {
  NOTIMPLEMENTED();
  return nullptr;
}

ash::AssistantStateBase* FakeServiceContext::assistant_state() {
  DCHECK(assistant_state_ != nullptr);
  return assistant_state_;
}

CrasAudioHandler* FakeServiceContext::cras_audio_handler() {
  NOTIMPLEMENTED();
  return nullptr;
}

DeviceActions* FakeServiceContext::device_actions() {
  return DeviceActions::Get();
}

scoped_refptr<base::SequencedTaskRunner>
FakeServiceContext::main_task_runner() {
  DCHECK(main_task_runner_ != nullptr);
  return main_task_runner_;
}

PowerManagerClient* FakeServiceContext::power_manager_client() {
  DCHECK(power_manager_client_ != nullptr);
  return power_manager_client_;
}

std::string FakeServiceContext::primary_account_gaia_id() {
  return gaia_id_;
}

}  // namespace assistant
}  // namespace chromeos
