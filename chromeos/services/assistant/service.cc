// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
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
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

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

// Testing override for the AssistantSettingsManager implementation.
AssistantSettingsManager* g_settings_manager_override = nullptr;

}  // namespace

Service::Service(mojo::PendingReceiver<mojom::AssistantService> receiver,
                 std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                     url_loader_factory_info)
    : receiver_(this, std::move(receiver)),
      token_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      url_loader_factory_info_(std::move(url_loader_factory_info)) {
  // TODO(xiaohuic): in MASH we will need to setup the dbus client if assistant
  // service runs in its own process.
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();
}

Service::~Service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  assistant_state_.RemoveObserver(this);
  auto* const session_controller = ash::SessionController::Get();
  if (observing_ash_session_ && session_controller) {
    session_controller->RemoveSessionActivationObserverForAccountId(account_id_,
                                                                    this);
  }
}

// static
void Service::OverrideSettingsManagerForTesting(
    AssistantSettingsManager* manager) {
  g_settings_manager_override = manager;
}

void Service::RequestAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Bypass access token fetching under signed out mode.
  if (is_signed_out_mode_)
    return;

  VLOG(1) << "Start requesting access token.";
  GetIdentityAccessor()->GetPrimaryAccountInfo(base::BindOnce(
      &Service::GetPrimaryAccountInfoCallback, base::Unretained(this)));
}

bool Service::ShouldEnableHotword() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool dsp_available = chromeos::CrasAudioHandler::Get()->HasHotwordDevice();

  // Disable hotword if hotword is not set to always on and power source is not
  // connected.
  if (!dsp_available && !assistant_state_.hotword_always_on().value_or(false) &&
      !power_source_connected_) {
    return false;
  }

  return assistant_state_.hotword_enabled().value();
}

void Service::SetIdentityAccessorForTesting(
    identity::mojom::IdentityAccessorPtr identity_accessor) {
  identity_accessor_ = std::move(identity_accessor);
}

void Service::SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) {
  token_refresh_timer_ = std::move(timer);
}

AssistantStateProxy* Service::GetAssistantStateProxyForTesting() {
  return &assistant_state_;
}

void Service::Init(mojo::PendingRemote<mojom::Client> client,
                   mojo::PendingRemote<mojom::DeviceActions> device_actions,
                   bool is_test) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_test_ = is_test;
  client_.Bind(std::move(client));
  device_actions_.Bind(std::move(device_actions));

  assistant_state_.Init(client_.get());
  assistant_state_.AddObserver(this);

  DCHECK(!assistant_manager_service_);

  // Don't fetch token for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    is_signed_out_mode_ = true;
    return;
  }

  RequestAccessToken();
}

void Service::BindAssistant(mojo::PendingReceiver<mojom::Assistant> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(assistant_manager_service_);
  assistant_receivers_.Add(assistant_manager_service_.get(),
                           std::move(receiver));
}

void Service::BindSettingsManager(
    mojo::PendingReceiver<mojom::AssistantSettingsManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (g_settings_manager_override) {
    g_settings_manager_override->BindRequest(std::move(receiver));
    return;
  }

  DCHECK(assistant_manager_service_);
  assistant_manager_service_->GetAssistantSettingsManager()->BindRequest(
      std::move(receiver));
}

void Service::PowerChanged(const power_manager::PowerSupplyProperties& prop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool power_source_connected =
      prop.external_power() == power_manager::PowerSupplyProperties::AC;
  if (power_source_connected == power_source_connected_)
    return;

  power_source_connected_ = power_source_connected;
  UpdateAssistantManagerState();
}

void Service::SuspendDone(const base::TimeDelta& sleep_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |token_refresh_timer_| may become stale during sleeping, so we immediately
  // request a new token to make sure it is fresh.
  if (token_refresh_timer_->IsRunning()) {
    token_refresh_timer_->AbandonAndStop();
    RequestAccessToken();
  }
}

void Service::OnSessionActivated(bool activated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  locked_ = locked;
  UpdateListeningState();
}

void Service::OnAssistantHotwordAlwaysOn(bool hotword_always_on) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No need to update hotword status if power source is connected.
  if (power_source_connected_)
    return;

  UpdateAssistantManagerState();
}

void Service::OnAssistantSettingsEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnAssistantHotwordEnabled(bool enabled) {
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

void Service::UpdateAssistantManagerState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!assistant_state_.hotword_enabled().has_value() ||
      !assistant_state_.settings_enabled().has_value() ||
      !assistant_state_.locale().has_value() ||
      (!access_token_.has_value() && !is_signed_out_mode_) ||
      !assistant_state_.arc_play_store_enabled().has_value()) {
    // Assistant state has not finished initialization, let's wait.
    return;
  }

  if (!assistant_manager_service_)
    CreateAssistantManagerService();

  auto state = assistant_manager_service_->GetState();
  switch (state) {
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
    case AssistantManagerService::State::STARTED:
      // Wait if |assistant_manager_service_| is not at a stable state.
      update_assistant_manager_callback_.Cancel();
      update_assistant_manager_callback_.Reset(
          base::BindOnce(&Service::UpdateAssistantManagerState,
                         weak_ptr_factory_.GetWeakPtr()));
      main_task_runner_->PostDelayedTask(
          FROM_HERE, update_assistant_manager_callback_.callback(),
          kUpdateAssistantManagerDelay);
      break;
    case AssistantManagerService::State::RUNNING:
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

identity::mojom::IdentityAccessor* Service::GetIdentityAccessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!identity_accessor_)
    client_->RequestIdentityAccessor(mojo::MakeRequest(&identity_accessor_));
  return identity_accessor_.get();
}

void Service::GetPrimaryAccountInfoCallback(
    const base::Optional<CoreAccountId>& account_id,
    const base::Optional<std::string>& gaia,
    const base::Optional<std::string>& email,
    const identity::AccountState& account_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Validate the remotely-supplied parameters before using them below: if
  // |account_id| is non-null, the other two should be non-null as well per
  // the stated contract of IdentityAccessor::GetPrimaryAccountInfo().
  CHECK((!account_id.has_value() || (gaia.has_value() && email.has_value())));

  if (!account_id.has_value() || !account_state.has_refresh_token ||
      gaia->empty()) {
    LOG(ERROR) << "Failed to retrieve primary account info.";
    RetryRefreshToken();
    return;
  }
  account_id_ = user_manager::known_user::GetAccountId(*email, *gaia,
                                                       AccountType::GOOGLE);
  identity::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(kScopeAuthGcm);
  if (features::IsClearCutLogEnabled())
    scopes.insert(kScopeClearCutLog);
  identity_accessor_->GetAccessToken(
      *account_id, scopes, "cros_assistant",
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)));
}

void Service::GetAccessTokenCallback(const base::Optional<std::string>& token,
                                     base::Time expiration_time,
                                     const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  if (is_test_) {
    // Use fake service in browser tests.
    assistant_manager_service_ =
        std::make_unique<FakeAssistantManagerServiceImpl>();
    return;
  }

  DCHECK(client_);

  mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor;
  client_->RequestBatteryMonitor(
      battery_monitor.InitWithNewPipeAndPassReceiver());

  // |assistant_manager_service_| is only created once.
  DCHECK(url_loader_factory_info_);
  assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
      client_.get(), std::move(battery_monitor), this,
      std::move(url_loader_factory_info_));
#else
  assistant_manager_service_ =
      std::make_unique<FakeAssistantManagerServiceImpl>();
#endif
}

void Service::FinalizeAssistantManagerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);

  // Using session_observer_binding_ as a flag to control onetime initialization
  if (!observing_ash_session_) {
    // Bind to the AssistantController in ash.
    client_->RequestAssistantController(
        mojo::MakeRequest(&assistant_controller_));
    mojo::PendingRemote<mojom::Assistant> remote_for_controller;
    BindAssistant(remote_for_controller.InitWithNewPipeAndPassReceiver());
    assistant_controller_->SetAssistant(std::move(remote_for_controller));

    if (features::IsTimerNotificationEnabled()) {
      // Bind to the AssistantAlarmTimerController in ash.
      client_->RequestAssistantAlarmTimerController(
          assistant_alarm_timer_controller_.BindNewPipeAndPassReceiver());
    }

    // Bind to the AssistantNotificationController in ash.
    client_->RequestAssistantNotificationController(
        assistant_notification_controller_.BindNewPipeAndPassReceiver());

    // Bind to the AssistantScreenContextController in ash.
    client_->RequestAssistantScreenContextController(
        assistant_screen_context_controller_.BindNewPipeAndPassReceiver());

    AddAshSessionObserver();
  }

  client_->OnAssistantStatusChanged(true /* running */);
  UpdateListeningState();
  DVLOG(1) << "Assistant is running";
}

void Service::StopAssistantManagerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  assistant_manager_service_->Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  client_->OnAssistantStatusChanged(false /* running */);
}

void Service::AddAshSessionObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observing_ash_session_ = true;
  // No session controller in unittest.
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->AddSessionActivationObserverForAccountId(
        account_id_, this);
  }
}

void Service::UpdateListeningState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
