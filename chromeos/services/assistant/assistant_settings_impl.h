// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
class AssistantController;
class AssistantStateBase;
}  // namespace ash

namespace assistant_client {
struct SpeakerIdEnrollmentStatus;
struct SpeakerIdEnrollmentUpdate;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerServiceImpl;
class ServiceContext;

class AssistantSettingsImpl : public AssistantSettings {
 public:
  AssistantSettingsImpl(ServiceContext* context,
                        AssistantManagerServiceImpl* assistant_manager_service);
  ~AssistantSettingsImpl() override;

  bool speaker_id_enrollment_done() { return speaker_id_enrollment_done_; }

  // AssistantSettings overrides:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override;
  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override;
  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      base::WeakPtr<SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment() override;
  void SyncSpeakerIdEnrollmentStatus() override;

  void SyncDeviceAppsStatus(base::OnceCallback<void(bool)> callback);

  void UpdateServerDeviceSettings();

 private:
  void HandleSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update);
  void HandleStopSpeakerIdEnrollment();
  void HandleSpeakerIdEnrollmentStatusSync(
      const assistant_client::SpeakerIdEnrollmentStatus& status);
  void HandleDeviceAppsStatusSync(base::OnceCallback<void(bool)> callback,
                                  const std::string& settings);

  ash::AssistantStateBase* assistant_state();
  ash::AssistantController* assistant_controller();
  scoped_refptr<base::SequencedTaskRunner> main_task_runner();

  ServiceContext* const context_;
  AssistantManagerServiceImpl* const assistant_manager_service_;
  base::WeakPtr<SpeakerIdEnrollmentClient> speaker_id_enrollment_client_;

  // Whether the speaker id enrollment has complete for the user.
  bool speaker_id_enrollment_done_ = false;

  base::WeakPtrFactory<AssistantSettingsImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSettingsImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_
