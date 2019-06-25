// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform_api_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/system/sys_info.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/platform/power_manager_provider_impl.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/utils.h"
#include "libassistant/shared/public/assistant_export.h"
#include "libassistant/shared/public/platform_api.h"
#include "libassistant/shared/public/platform_factory.h"
#include "media/audio/audio_device_description.h"

using assistant_client::AudioInputProvider;
using assistant_client::AudioOutputProvider;
using assistant_client::AuthProvider;
using assistant_client::FileProvider;
using assistant_client::NetworkProvider;
using assistant_client::PlatformApi;
using assistant_client::SystemProvider;

namespace chromeos {
namespace assistant {

////////////////////////////////////////////////////////////////////////////////
// DummyAuthProvider
////////////////////////////////////////////////////////////////////////////////

std::string PlatformApiImpl::DummyAuthProvider::GetAuthClientId() {
  return "kFakeClientId";
}

std::vector<std::string>
PlatformApiImpl::DummyAuthProvider::GetClientCertificateChain() {
  return {};
}

void PlatformApiImpl::DummyAuthProvider::CreateCredentialAttestationJwt(
    const std::string& authorization_code,
    const std::vector<std::pair<std::string, std::string>>& claims,
    CredentialCallback attestation_callback) {
  attestation_callback(Error::SUCCESS, "", "");
}

void PlatformApiImpl::DummyAuthProvider::CreateRefreshAssertionJwt(
    const std::string& key_identifier,
    const std::vector<std::pair<std::string, std::string>>& claims,
    AssertionCallback assertion_callback) {
  assertion_callback(Error::SUCCESS, "");
}

void PlatformApiImpl::DummyAuthProvider::CreateDeviceAttestationJwt(
    const std::vector<std::pair<std::string, std::string>>& claims,
    AssertionCallback attestation_callback) {
  attestation_callback(Error::SUCCESS, "");
}

std::string
PlatformApiImpl::DummyAuthProvider::GetAttestationCertFingerprint() {
  return "kFakeAttestationCertFingerprint";
}

void PlatformApiImpl::DummyAuthProvider::RemoveCredentialKey(
    const std::string& key_identifier) {}

void PlatformApiImpl::DummyAuthProvider::Reset() {}

////////////////////////////////////////////////////////////////////////////////
// PlatformApiImpl
////////////////////////////////////////////////////////////////////////////////

PlatformApiImpl::PlatformApiImpl(
    service_manager::Connector* connector,
    AssistantMediaSession* media_session,
    device::mojom::BatteryMonitorPtr battery_monitor,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
    std::string pref_locale)
    : audio_input_provider_(connector,
                            media::AudioDeviceDescription::kDefaultDeviceId,
                            /*hotword_device_id=*/std::string()),
      audio_output_provider_(connector,
                             media_session,
                             background_task_runner,
                             media::AudioDeviceDescription::kDefaultDeviceId),
      network_provider_(connector),
      pref_locale_(pref_locale) {
  // Only enable native power features if they are supported by the UI.
  std::unique_ptr<PowerManagerProviderImpl> provider;
  if (features::IsPowerManagerEnabled()) {
    provider = std::make_unique<PowerManagerProviderImpl>(
        connector, std::move(main_thread_task_runner));
  }
  system_provider_ = std::make_unique<SystemProviderImpl>(
      std::move(provider), std::move(battery_monitor));

  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  OnAudioNodesChanged();
}

PlatformApiImpl::~PlatformApiImpl() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

AudioInputProviderImpl& PlatformApiImpl::GetAudioInputProvider() {
  return audio_input_provider_;
}

AudioOutputProvider& PlatformApiImpl::GetAudioOutputProvider() {
  return audio_output_provider_;
}

AuthProvider& PlatformApiImpl::GetAuthProvider() {
  return auth_provider_;
}

FileProvider& PlatformApiImpl::GetFileProvider() {
  return file_provider_;
}

NetworkProvider& PlatformApiImpl::GetNetworkProvider() {
  return network_provider_;
}

SystemProvider& PlatformApiImpl::GetSystemProvider() {
  return *system_provider_;
}

void PlatformApiImpl::OnAudioNodesChanged() {
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;
  auto* cras = chromeos::CrasAudioHandler::Get();

  chromeos::AudioDeviceList devices;
  cras->GetAudioDevices(&devices);

  const chromeos::AudioDevice* input_device = nullptr;
  const chromeos::AudioDevice* hotword_device = nullptr;

  for (const chromeos::AudioDevice& device : devices) {
    if (!device.is_input)
      continue;

    switch (device.type) {
      case chromeos::AUDIO_TYPE_USB:
      case chromeos::AUDIO_TYPE_HEADPHONE:
      case chromeos::AUDIO_TYPE_INTERNAL_MIC:
        if (!input_device || input_device->priority < device.priority)
          input_device = &device;
        break;
      case chromeos::AUDIO_TYPE_HOTWORD:
        if (!hotword_device || hotword_device->priority < device.priority)
          hotword_device = &device;
        break;
      default:
        // ignore other devices
        break;
    }
  }
  if (input_device)
    audio_input_provider_.SetDeviceId(base::NumberToString(input_device->id));
  if (hotword_device) {
    audio_input_provider_.SetHotwordDeviceId(
        base::NumberToString(hotword_device->id));
    audio_input_provider_.SetDspHotwordLocale(pref_locale_);
  }
}

void PlatformApiImpl::SetMicState(bool mic_open) {
  audio_input_provider_.SetMicState(mic_open);
}

void PlatformApiImpl::OnHotwordEnabled(bool enable) {
  audio_input_provider_.OnHotwordEnabled(enable);
}

}  // namespace assistant
}  // namespace chromeos
