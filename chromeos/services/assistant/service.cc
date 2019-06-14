// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/fake_assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/public/features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/utils.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#endif

namespace chromeos {
namespace assistant {

namespace {

constexpr char kScopeAuthGcm[] = "https://www.googleapis.com/auth/gcm";
constexpr char kScopeAssistant[] =
    "https://www.googleapis.com/auth/assistant-sdk-prototype";
constexpr char kScopeClearCutLog[] = "https://www.googleapis.com/auth/cclog";

constexpr base::TimeDelta kMinTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kMaxTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(60 * 1000);

}  // namespace

Service::Service(service_manager::mojom::ServiceRequest request,
                 network::NetworkConnectionTracker* network_connection_tracker,
                 std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                     url_loader_factory_info)
    : service_binding_(this, std::move(request)),
      platform_binding_(this),
      token_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      power_manager_observer_(this),
      network_connection_tracker_(network_connection_tracker),
      url_loader_factory_info_(std::move(url_loader_factory_info)),
      weak_ptr_factory_(this) {
  registry_.AddInterface<mojom::AssistantPlatform>(base::BindRepeating(
      &Service::BindAssistantPlatformConnection, base::Unretained(this)));

  // TODO(xiaohuic): in MASH we will need to setup the dbus client if assistant
  // service runs in its own process.
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();
}

Service::~Service() {
  auto* const session_controller = ash::SessionController::Get();
  if (observing_ash_session_ && session_controller) {
    session_controller->RemoveSessionActivationObserverForAccountId(account_id_,
                                                                    this);
  }
}

void Service::RequestAccessToken() {
  // Bypass access token fetching under signed out mode.
  if (is_signed_out_mode_)
    return;

  VLOG(1) << "Start requesting access token.";
  GetIdentityAccessor()->GetPrimaryAccountInfo(base::BindOnce(
      &Service::GetPrimaryAccountInfoCallback, base::Unretained(this)));
}

bool Service::ShouldEnableHotword() {
  bool dsp_available = false;
  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (const chromeos::AudioDevice& device : devices) {
    if (device.type == chromeos::AUDIO_TYPE_HOTWORD) {
      dsp_available = true;
    }
  }

  // Disable hotword if hotword is not set to always on and power source is not
  // connected.
  if (!dsp_available && !assistant_state_.hotword_always_on().value() &&
      !power_source_connected_) {
    return false;
  }

  return assistant_state_.hotword_enabled().value();
}

void Service::SetIdentityAccessorForTesting(
    identity::mojom::IdentityAccessorPtr identity_accessor) {
  identity_accessor_ = std::move(identity_accessor);
}

void Service::SetAssistantManagerForTesting(
    std::unique_ptr<AssistantManagerService> assistant_manager_service) {
  assistant_manager_service_ = std::move(assistant_manager_service);
}

void Service::SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) {
  token_refresh_timer_ = std::move(timer);
}

void Service::OnStart() {}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void Service::BindAssistantConnection(mojom::AssistantRequest request) {
  DCHECK(assistant_manager_service_);
  bindings_.AddBinding(assistant_manager_service_.get(), std::move(request));
}

void Service::BindAssistantPlatformConnection(
    mojom::AssistantPlatformRequest request) {
  platform_binding_.Bind(std::move(request));
}

void Service::PowerChanged(const power_manager::PowerSupplyProperties& prop) {
  const bool power_source_connected =
      prop.external_power() == power_manager::PowerSupplyProperties::AC;
  if (power_source_connected == power_source_connected_)
    return;

  power_source_connected_ = power_source_connected;
  UpdateAssistantManagerState();
}

void Service::SuspendDone(const base::TimeDelta& sleep_duration) {
  // |token_refresh_timer_| may become stale during sleeping, so we immediately
  // request a new token to make sure it is fresh.
  if (token_refresh_timer_->IsRunning()) {
    token_refresh_timer_->AbandonAndStop();
    RequestAccessToken();
  }
}

void Service::OnSessionActivated(bool activated) {
  DCHECK(client_);
  session_active_ = activated;

  if (assistant_manager_service_->GetState() !=
      AssistantManagerService::State::RUNNING) {
    return;
  }

  client_->OnAssistantStatusChanged(activated /* running */);
  UpdateListeningState();
}

void Service::OnLockStateChanged(bool locked) {
  locked_ = locked;
  UpdateListeningState();
}

void Service::OnVoiceInteractionSettingsEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnVoiceInteractionHotwordEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnLocaleChanged(const std::string& locale) {
  UpdateAssistantManagerState();
}

void Service::OnArcPlayStoreEnabledChanged(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnLockedFullScreenStateChanged(bool enabled) {
  UpdateListeningState();
}

void Service::OnVoiceInteractionHotwordAlwaysOn(bool always_on) {
  // No need to update hotword status if power source is connected.
  if (power_source_connected_)
    return;

  UpdateAssistantManagerState();
}

void Service::UpdateAssistantManagerState() {
  if (!assistant_state_.hotword_enabled().has_value() ||
      !assistant_state_.settings_enabled().has_value() ||
      !assistant_state_.hotword_always_on().has_value() ||
      !assistant_state_.locale().has_value() ||
      (!access_token_.has_value() && !is_signed_out_mode_) ||
      !assistant_state_.arc_play_store_enabled().has_value()) {
    // Assistant state has not finished initialization, let's wait.
    return;
  }

  if (!assistant_manager_service_)
    CreateAssistantManagerService();

  switch (assistant_manager_service_->GetState()) {
    case AssistantManagerService::State::STOPPED:
      if (assistant_state_.settings_enabled().value()) {
        assistant_manager_service_->Start(
            is_signed_out_mode_ ? base::nullopt : access_token_,
            ShouldEnableHotword(),
            base::BindOnce(
                [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                   base::OnceCallback<void()> callback) {
                  task_runner->PostTask(FROM_HERE, std::move(callback));
                },
                main_task_runner_,
                base::BindOnce(&Service::FinalizeAssistantManagerService,
                               weak_ptr_factory_.GetWeakPtr())));
        DVLOG(1) << "Request Assistant start";
      }
      break;
    case AssistantManagerService::State::RUNNING:
    case AssistantManagerService::State::STARTED:
      if (assistant_state_.settings_enabled().value()) {
        if (!is_signed_out_mode_)
          assistant_manager_service_->SetAccessToken(access_token_.value());
        assistant_manager_service_->EnableHotword(ShouldEnableHotword());
        assistant_manager_service_->SetArcPlayStoreEnabled(
            assistant_state_.arc_play_store_enabled().value());
      } else {
        StopAssistantManagerService();
      }
      break;
  }
}

void Service::BindAssistantSettingsManager(
    mojom::AssistantSettingsManagerRequest request) {
  DCHECK(assistant_manager_service_);
  assistant_manager_service_->GetAssistantSettingsManager()->BindRequest(
      std::move(request));
}

void Service::Init(mojom::ClientPtr client,
                   mojom::DeviceActionsPtr device_actions,
                   bool is_test) {
  is_test_ = is_test;
  client_ = std::move(client);
  device_actions_ = std::move(device_actions);
  assistant_state_.Init(service_binding_.GetConnector());
  assistant_state_.AddObserver(this);

  // Don't fetch token for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    is_signed_out_mode_ = true;
    return;
  }

  RequestAccessToken();
}

identity::mojom::IdentityAccessor* Service::GetIdentityAccessor() {
  if (!identity_accessor_) {
    service_binding_.GetConnector()->BindInterface(
        identity::mojom::kServiceName, mojo::MakeRequest(&identity_accessor_));
  }
  return identity_accessor_.get();
}

void Service::GetPrimaryAccountInfoCallback(
    const base::Optional<CoreAccountInfo>& account_info,
    const identity::AccountState& account_state) {
  if (!account_info.has_value() || !account_state.has_refresh_token ||
      account_info.value().gaia.empty()) {
    LOG(ERROR) << "Failed to retrieve primary account info.";
    RetryRefreshToken();
    return;
  }
  account_id_ = AccountId::FromUserEmailGaiaId(account_info.value().email,
                                               account_info.value().gaia);
  identity::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(kScopeAuthGcm);
  if (features::IsClearCutLogEnabled())
    scopes.insert(kScopeClearCutLog);
  identity_accessor_->GetAccessToken(
      account_info.value().account_id, scopes, "cros_assistant",
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)));
}

void Service::GetAccessTokenCallback(const base::Optional<std::string>& token,
                                     base::Time expiration_time,
                                     const GoogleServiceAuthError& error) {
  if (!token.has_value()) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    RetryRefreshToken();
    return;
  }

  access_token_ = token;
  UpdateAssistantManagerState();
  token_refresh_timer_->Start(FROM_HERE, expiration_time - base::Time::Now(),
                              this, &Service::RequestAccessToken);
}

void Service::RetryRefreshToken() {
  base::TimeDelta backoff_delay =
      std::min(kMinTokenRefreshDelay *
                   (1 << (token_refresh_error_backoff_factor - 1)),
               kMaxTokenRefreshDelay) +
      base::RandDouble() * kMinTokenRefreshDelay;
  if (backoff_delay < kMaxTokenRefreshDelay)
    ++token_refresh_error_backoff_factor;
  token_refresh_timer_->Start(FROM_HERE, backoff_delay, this,
                              &Service::RequestAccessToken);
}

void Service::CreateAssistantManagerService() {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  if (is_test_) {
    // Use fake service in browser tests.
    assistant_manager_service_ =
        std::make_unique<FakeAssistantManagerServiceImpl>();
    return;
  }

  device::mojom::BatteryMonitorPtr battery_monitor;
  service_binding_.GetConnector()->BindInterface(
      device::mojom::kServiceName, mojo::MakeRequest(&battery_monitor));

  // |assistant_manager_service_| is only created once.
  DCHECK(url_loader_factory_info_);
  assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
      service_binding_.GetConnector(), std::move(battery_monitor), this,
      network_connection_tracker_, std::move(url_loader_factory_info_));
#else
  assistant_manager_service_ =
      std::make_unique<FakeAssistantManagerServiceImpl>();
#endif
}

void Service::FinalizeAssistantManagerService() {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);

  // Using session_observer_binding_ as a flag to control onetime initialization
  if (!observing_ash_session_) {
    // Bind to the AssistantController in ash.
    service_binding_.GetConnector()->BindInterface(ash::mojom::kServiceName,
                                                   &assistant_controller_);

    mojom::AssistantPtr ptr;
    BindAssistantConnection(mojo::MakeRequest(&ptr));
    assistant_controller_->SetAssistant(std::move(ptr));

    if (features::IsTimerNotificationEnabled()) {
      // Bind to the AssistantAlarmTimerController in ash.
      service_binding_.GetConnector()->BindInterface(
          ash::mojom::kServiceName, &assistant_alarm_timer_controller_);
    }

    // Bind to the AssistantNotificationController in ash.
    service_binding_.GetConnector()->BindInterface(
        ash::mojom::kServiceName, &assistant_notification_controller_);

    // Bind to the AssistantScreenContextController in ash.
    service_binding_.GetConnector()->BindInterface(
        ash::mojom::kServiceName, &assistant_screen_context_controller_);

    registry_.AddInterface<mojom::Assistant>(base::BindRepeating(
        &Service::BindAssistantConnection, base::Unretained(this)));

    registry_.AddInterface<mojom::AssistantSettingsManager>(base::BindRepeating(
        &Service::BindAssistantSettingsManager, base::Unretained(this)));

    AddAshSessionObserver();
  }

  client_->OnAssistantStatusChanged(true /* running */);
  UpdateListeningState();
  DVLOG(1) << "Assistant is running";
}

void Service::StopAssistantManagerService() {
  assistant_manager_service_->Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  client_->OnAssistantStatusChanged(false /* running */);
}

void Service::AddAshSessionObserver() {
  observing_ash_session_ = true;
  ash::SessionController::Get()->AddSessionActivationObserverForAccountId(
      account_id_, this);
}

void Service::UpdateListeningState() {
  if (assistant_manager_service_->GetState() !=
      AssistantManagerService::State::RUNNING) {
    return;
  }

  bool should_listen =
      !locked_ &&
      !assistant_state_.locked_full_screen_enabled().value_or(false) &&
      session_active_;
  DVLOG(1) << "Update assistant listening state: " << should_listen;
  assistant_manager_service_->EnableListening(should_listen);
}

}  // namespace assistant
}  // namespace chromeos
