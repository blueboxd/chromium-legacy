// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/unlock_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/metrics.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_monitor_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace proximity_auth {
namespace {

// The maximum amount of time that the unlock manager can stay in the 'waking
// up' state after resuming from sleep.
constexpr base::TimeDelta kWakingUpDuration = base::TimeDelta::FromSeconds(15);

// The maximum amount of time that we wait for the BluetoothAdapter to be
// fully initialized after resuming from sleep.
// TODO(crbug.com/986896): This is necessary because the BluetoothAdapter
// returns incorrect presence and power values directly after resume, and does
// not return correct values until about 1-2 seconds later. Remove this once
// the bug is fixed.
constexpr base::TimeDelta kBluetoothAdapterResumeMaxDuration =
    base::TimeDelta::FromSeconds(3);

// The limit on the elapsed time for an auth attempt. If an auth attempt exceeds
// this limit, it will time out and be rejected. This is provided as a failsafe,
// in case something goes wrong.
constexpr base::TimeDelta kAuthAttemptTimeout = base::TimeDelta::FromSeconds(5);

constexpr base::TimeDelta kMinGetUnlockableRemoteStatusDuration =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kMaxGetUnlockableRemoteStatusDuration =
    base::TimeDelta::FromSeconds(15);
const int kNumDurationMetricBuckets = 100;

// Returns the remote device's security settings state, for metrics,
// corresponding to a remote status update.
metrics::RemoteSecuritySettingsState GetRemoteSecuritySettingsState(
    const RemoteStatusUpdate& status_update) {
  switch (status_update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return metrics::RemoteSecuritySettingsState::UNKNOWN;

    case SECURE_SCREEN_LOCK_DISABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_ENABLED;
      }

    case SECURE_SCREEN_LOCK_ENABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_ENABLED;
      }
  }

  NOTREACHED();
  return metrics::RemoteSecuritySettingsState::UNKNOWN;
}

void RecordGetRemoteStatusResultFailure(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    SmartLockMetricsRecorder::SmartLockGetRemoteStatusResultFailureReason
        failure_reason) {
  if (screenlock_type == ProximityAuthSystem::SESSION_LOCK) {
    SmartLockMetricsRecorder::RecordGetRemoteStatusResultUnlockFailure(
        failure_reason);
  } else if (screenlock_type == ProximityAuthSystem::SIGN_IN) {
    SmartLockMetricsRecorder::RecordGetRemoteStatusResultSignInFailure(
        failure_reason);
  }
}

void RecordAuthResultFailure(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    SmartLockMetricsRecorder::SmartLockAuthResultFailureReason failure_reason) {
  if (screenlock_type == ProximityAuthSystem::SESSION_LOCK) {
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(failure_reason);
  } else if (screenlock_type == ProximityAuthSystem::SIGN_IN) {
    SmartLockMetricsRecorder::RecordAuthResultSignInFailure(failure_reason);
  }
}

}  // namespace

UnlockManagerImpl::UnlockManagerImpl(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client)
    : screenlock_type_(screenlock_type),
      proximity_auth_client_(proximity_auth_client),
      bluetooth_suspension_recovery_timer_(
          std::make_unique<base::OneShotTimer>()) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::BindOnce(&UnlockManagerImpl::OnBluetoothAdapterInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

UnlockManagerImpl::~UnlockManagerImpl() {
  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
  if (proximity_monitor_)
    proximity_monitor_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

bool UnlockManagerImpl::IsUnlockAllowed() {
  return (remote_screenlock_state_ &&
          *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED &&
          life_cycle_ &&
          life_cycle_->GetState() ==
              RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED &&
          proximity_monitor_ && proximity_monitor_->IsUnlockAllowed() &&
          (screenlock_type_ != ProximityAuthSystem::SIGN_IN ||
           (GetMessenger() && GetMessenger()->SupportsSignIn())));
}

void UnlockManagerImpl::SetRemoteDeviceLifeCycle(
    RemoteDeviceLifeCycle* life_cycle) {
  PA_LOG(VERBOSE) << "Request received to change scan state to: "
                  << (life_cycle == nullptr ? "inactive" : "active") << ".";

  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);

  life_cycle_ = life_cycle;
  if (life_cycle_) {
    life_cycle_->AddObserver(this);

    attempt_secure_connection_start_time_ =
        base::DefaultClock::GetInstance()->Now();

    SetIsPerformingInitialScan(true /* is_performing_initial_scan */);
    AttemptToStartRemoteDeviceLifecycle();
  } else {
    ResetPerformanceMetricsTimestamps();

    if (proximity_monitor_)
      proximity_monitor_->RemoveObserver(this);
    proximity_monitor_.reset();
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  remote_screenlock_state_.reset();
  if (new_state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    DCHECK(life_cycle_->GetChannel());
    DCHECK(GetMessenger());
    if (!proximity_monitor_) {
      proximity_monitor_ = CreateProximityMonitor(life_cycle_);
      proximity_monitor_->AddObserver(this);
      proximity_monitor_->Start();
    }
    GetMessenger()->AddObserver(this);

    attempt_get_remote_status_start_time_ =
        base::DefaultClock::GetInstance()->Now();
  } else if (proximity_monitor_) {
    proximity_monitor_->RemoveObserver(this);
    proximity_monitor_->Stop();
    proximity_monitor_.reset();
  }

  if (new_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    SetIsPerformingInitialScan(false /* is_performing_initial_scan */);

  UpdateLockScreen();
}

void UnlockManagerImpl::OnUnlockEventSent(bool success) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Sent easy_unlock event, but no auth attempted.";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockEventSentButNotAttemptingAuth);
  } else if (success) {
    FinalizeAuthAttempt(base::nullopt /* failure_reason */);
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedtoNotifyHostDeviceThatSmartLockWasUsed);
  }
}

void UnlockManagerImpl::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(VERBOSE) << "Status Update: ("
                  << "user_present=" << status_update.user_presence << ", "
                  << "secure_screen_lock="
                  << status_update.secure_screen_lock_state << ", "
                  << "trust_agent=" << status_update.trust_agent_state << ")";
  metrics::RecordRemoteSecuritySettingsState(
      GetRemoteSecuritySettingsState(status_update));

  remote_screenlock_state_.reset(new RemoteScreenlockState(
      GetScreenlockStateFromRemoteUpdate(status_update)));

  // This also calls |UpdateLockScreen()|
  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::OnDecryptResponse(const std::string& decrypted_bytes) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Decrypt response received but not attempting auth.";
    return;
  }

  if (decrypted_bytes.empty()) {
    PA_LOG(WARNING) << "Failed to decrypt sign-in challenge.";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedToDecryptSignInChallenge);
  } else {
    sign_in_secret_.reset(new std::string(decrypted_bytes));
    if (GetMessenger())
      GetMessenger()->DispatchUnlockEvent();
  }
}

void UnlockManagerImpl::OnUnlockResponse(bool success) {
  if (!is_attempting_auth_) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockRequestSentButNotAttemptingAuth);
    PA_LOG(ERROR) << "Unlock request sent but not attempting auth.";
    return;
  }

  PA_LOG(INFO) << "Received unlock response from device: "
               << (success ? "yes" : "no") << ".";

  if (success && GetMessenger()) {
    GetMessenger()->DispatchUnlockEvent();
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedToSendUnlockRequest);
  }
}

void UnlockManagerImpl::OnDisconnected() {
  if (is_attempting_auth_) {
    RecordAuthResultFailure(
        screenlock_type_,
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kAuthenticatedChannelDropped);
  } else {
    RecordGetRemoteStatusResultFailure(
        screenlock_type_,
        SmartLockMetricsRecorder::SmartLockGetRemoteStatusResultFailureReason::
            kAuthenticatedChannelDropped);
  }

  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
}

void UnlockManagerImpl::OnProximityStateChanged() {
  PA_LOG(VERBOSE) << "Proximity state changed.";
  UpdateLockScreen();
}

void UnlockManagerImpl::OnBluetoothAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void UnlockManagerImpl::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                              bool present) {
  UpdateLockScreen();
}

void UnlockManagerImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                              bool powered) {
  UpdateLockScreen();
}

void UnlockManagerImpl::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // TODO(crbug.com/986896): For a short time window after resuming from
  // suspension, BluetoothAdapter returns incorrect presence and power values.
  // Cache the correct values now, in case we need to check those values during
  // that time window when the device resumes.
  was_bluetooth_present_and_powered_before_last_suspend_ =
      IsBluetoothPresentAndPowered();
  bluetooth_suspension_recovery_timer_->Stop();
}

void UnlockManagerImpl::SuspendDone(const base::TimeDelta& sleep_duration) {
  bluetooth_suspension_recovery_timer_->Start(
      FROM_HERE, kBluetoothAdapterResumeMaxDuration,
      base::Bind(&UnlockManagerImpl::UpdateLockScreen,
                 weak_ptr_factory_.GetWeakPtr()));

  SetIsPerformingInitialScan(true /* is_performing_initial_scan */);
}

bool UnlockManagerImpl::IsBluetoothPresentAndPowered() const {
  // TODO(crbug.com/986896): If the BluetoothAdapter is still "resuming after
  // suspension" at this time, it's prone to this bug, meaning we cannot trust
  // its returned presence and power values. If this is the case, depend on
  // the cached |was_bluetooth_present_and_powered_before_last_suspend_| to
  // signal if Bluetooth is enabled; otherwise, directly check request values
  // from BluetoothAdapter. Remove this check once the bug is fixed.
  if (IsBluetoothAdapterRecoveringFromSuspend())
    return was_bluetooth_present_and_powered_before_last_suspend_;

  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent() &&
         bluetooth_adapter_->IsPowered();
}

bool UnlockManagerImpl::IsBluetoothAdapterRecoveringFromSuspend() const {
  return bluetooth_suspension_recovery_timer_->IsRunning();
}

void UnlockManagerImpl::AttemptToStartRemoteDeviceLifecycle() {
  if (IsBluetoothPresentAndPowered() && life_cycle_ &&
      life_cycle_->GetState() == RemoteDeviceLifeCycle::State::STOPPED) {
    // If Bluetooth is disabled after this, |life_cycle_| will be notified by
    // SecureChannel that the connection attempt failed. From that point on,
    // |life_cycle_| will wait to be started again by UnlockManager.
    life_cycle_->Start();
  }
}

void UnlockManagerImpl::OnAuthAttempted(mojom::AuthType auth_type) {
  if (is_attempting_auth_) {
    PA_LOG(VERBOSE) << "Already attempting auth.";
    return;
  }

  if (auth_type != mojom::AuthType::USER_CLICK)
    return;

  is_attempting_auth_ = true;

  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "No life_cycle active when auth was attempted";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kNoPendingOrActiveHost);
    UpdateLockScreen();
    return;
  }

  if (!IsUnlockAllowed()) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockNotAllowed);
    UpdateLockScreen();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &UnlockManagerImpl::FinalizeAuthAttempt,
          reject_auth_attempt_weak_ptr_factory_.GetWeakPtr(),
          SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
              kAuthAttemptTimedOut),
      kAuthAttemptTimeout);

  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    SendSignInChallenge();
  } else {
    if (GetMessenger()->SupportsSignIn()) {
      GetMessenger()->RequestUnlock();
    } else {
      PA_LOG(VERBOSE)
          << "Protocol v3.1 not supported, skipping request_unlock.";
      GetMessenger()->DispatchUnlockEvent();
    }
  }
}

void UnlockManagerImpl::CancelConnectionAttempt() {
  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

std::unique_ptr<ProximityMonitor> UnlockManagerImpl::CreateProximityMonitor(
    RemoteDeviceLifeCycle* life_cycle) {
  return std::make_unique<ProximityMonitorImpl>(life_cycle->GetRemoteDevice(),
                                                life_cycle->GetChannel());
}

void UnlockManagerImpl::SendSignInChallenge() {
  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "Not ready to send sign-in challenge";
    return;
  }

  if (!GetMessenger()->GetChannel()) {
    PA_LOG(ERROR) << "Channel is not ready to send sign-in challenge.";
    return;
  }

  GetMessenger()->GetChannel()->GetConnectionMetadata(
      base::BindOnce(&UnlockManagerImpl::OnGetConnectionMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnlockManagerImpl::OnGetConnectionMetadata(
    chromeos::secure_channel::mojom::ConnectionMetadataPtr
        connection_metadata_ptr) {
  chromeos::multidevice::RemoteDeviceRef remote_device =
      life_cycle_->GetRemoteDevice();
  proximity_auth_client_->GetChallengeForUserAndDevice(
      remote_device.user_id(), remote_device.public_key(),
      connection_metadata_ptr->channel_binding_data,
      base::Bind(&UnlockManagerImpl::OnGotSignInChallenge,
                 weak_ptr_factory_.GetWeakPtr()));
}

void UnlockManagerImpl::OnGotSignInChallenge(const std::string& challenge) {
  PA_LOG(VERBOSE) << "Got sign-in challenge, sending for decryption...";
  if (GetMessenger())
    GetMessenger()->RequestDecryption(challenge);
}

ScreenlockState UnlockManagerImpl::GetScreenlockState() {
  if (!life_cycle_)
    return ScreenlockState::INACTIVE;

  if (!IsBluetoothPresentAndPowered())
    return ScreenlockState::NO_BLUETOOTH;

  if (IsUnlockAllowed())
    return ScreenlockState::AUTHENTICATED;

  RemoteDeviceLifeCycle::State life_cycle_state = life_cycle_->GetState();
  if (life_cycle_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    return ScreenlockState::PHONE_NOT_AUTHENTICATED;

  if (is_performing_initial_scan_)
    return ScreenlockState::BLUETOOTH_CONNECTING;

  Messenger* messenger = GetMessenger();

  // Show a timeout state if we can not connect to the remote device in a
  // reasonable amount of time.
  if (!is_performing_initial_scan_ && !messenger)
    return ScreenlockState::NO_PHONE;

  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN && messenger &&
      !messenger->SupportsSignIn())
    return ScreenlockState::PHONE_UNSUPPORTED;

  // If the RSSI is too low, then the remote device is nowhere near the local
  // device. This message should take priority over messages about screen lock
  // states.
  if (proximity_monitor_ && !proximity_monitor_->IsUnlockAllowed()) {
    if (remote_screenlock_state_ &&
        *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED) {
      return ScreenlockState::RSSI_TOO_LOW;
    } else {
      return ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW;
    }
  }

  if (remote_screenlock_state_) {
    switch (*remote_screenlock_state_) {
      case RemoteScreenlockState::DISABLED:
        return ScreenlockState::PHONE_NOT_LOCKABLE;

      case RemoteScreenlockState::LOCKED:
        return ScreenlockState::PHONE_LOCKED;

      case RemoteScreenlockState::UNKNOWN:
        return ScreenlockState::PHONE_UNSUPPORTED;

      case RemoteScreenlockState::PRIMARY_USER_ABSENT:
        return ScreenlockState::PRIMARY_USER_ABSENT;

      case RemoteScreenlockState::UNLOCKED:
        // Handled by the code below.
        break;
    }
  }

  if (messenger) {
    PA_LOG(WARNING) << "Connection to host established, but remote screenlock "
                    << "state was either malformed or not received.";
  }

  return ScreenlockState::NO_PHONE;
}

void UnlockManagerImpl::UpdateLockScreen() {
  AttemptToStartRemoteDeviceLifecycle();

  ScreenlockState new_state = GetScreenlockState();
  if (screenlock_state_ == new_state)
    return;

  PA_LOG(INFO) << "Updating screenlock state from " << screenlock_state_
               << " to " << new_state;

  if (new_state == ScreenlockState::AUTHENTICATED)
    RecordUnlockableRemoteStatusReceived();

  proximity_auth_client_->UpdateScreenlockState(new_state);
  screenlock_state_ = new_state;
}

void UnlockManagerImpl::SetIsPerformingInitialScan(
    bool is_performing_initial_scan) {
  PA_LOG(VERBOSE) << "Initial scan state is ["
                  << (is_performing_initial_scan_ ? "active" : "inactive")
                  << "]. Requesting state ["
                  << (is_performing_initial_scan ? "active" : "inactive")
                  << "].";

  is_performing_initial_scan_ = is_performing_initial_scan;

  // Clear the waking up state after a timeout.
  initial_scan_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
  if (is_performing_initial_scan_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UnlockManagerImpl::OnInitialScanTimeout,
                       initial_scan_timeout_weak_ptr_factory_.GetWeakPtr()),
        kWakingUpDuration);
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnInitialScanTimeout() {
  if (IsBluetoothPresentAndPowered()) {
    if (life_cycle_ &&
        life_cycle_->GetState() ==
            RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
      RecordGetRemoteStatusResultFailure(
          screenlock_type_, SmartLockMetricsRecorder::
                                SmartLockGetRemoteStatusResultFailureReason::
                                    kTimedOutDidNotReceiveRemoteStatusUpdate);
    } else {
      RecordGetRemoteStatusResultFailure(
          screenlock_type_,
          SmartLockMetricsRecorder::
              SmartLockGetRemoteStatusResultFailureReason::
                  kTimedOutCouldNotEstablishAuthenticatedChannel);
    }
  } else {
    RecordGetRemoteStatusResultFailure(
        screenlock_type_,
        SmartLockMetricsRecorder::SmartLockGetRemoteStatusResultFailureReason::
            kTimedOutBluetoothDisabled);
  }

  PA_LOG(INFO) << "Failed to connect to host within allotted time.";
  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::FinalizeAuthAttempt(
    const base::Optional<
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>& error) {
  if (error) {
    RecordAuthResultFailure(screenlock_type_, *error);
  }

  if (!is_attempting_auth_)
    return;

  // Cancel the pending task to time out the auth attempt.
  reject_auth_attempt_weak_ptr_factory_.InvalidateWeakPtrs();

  bool should_accept = !error;
  if (should_accept && proximity_monitor_)
    proximity_monitor_->RecordProximityMetricsOnAuthSuccess();

  is_attempting_auth_ = false;
  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    PA_LOG(VERBOSE) << "Finalizing sign-in...";
    proximity_auth_client_->FinalizeSignin(
        should_accept && sign_in_secret_ ? *sign_in_secret_ : std::string());
  } else {
    PA_LOG(VERBOSE) << "Finalizing unlock...";
    proximity_auth_client_->FinalizeUnlock(should_accept);
  }
}

UnlockManagerImpl::RemoteScreenlockState
UnlockManagerImpl::GetScreenlockStateFromRemoteUpdate(
    RemoteStatusUpdate update) {
  switch (update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_DISABLED:
      return RemoteScreenlockState::DISABLED;

    case SECURE_SCREEN_LOCK_ENABLED:
      if (update.user_presence == USER_PRESENCE_SECONDARY ||
          update.user_presence == USER_PRESENCE_BACKGROUND) {
        return RemoteScreenlockState::PRIMARY_USER_ABSENT;
      }

      if (update.user_presence == USER_PRESENT)
        return RemoteScreenlockState::UNLOCKED;

      return RemoteScreenlockState::LOCKED;

    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return RemoteScreenlockState::UNKNOWN;
  }

  NOTREACHED();
  return RemoteScreenlockState::UNKNOWN;
}

Messenger* UnlockManagerImpl::GetMessenger() {
  // TODO(tengs): We should use a weak pointer to hold the Messenger instance
  // instead.
  if (!life_cycle_)
    return nullptr;
  return life_cycle_->GetMessenger();
}

void UnlockManagerImpl::RecordUnlockableRemoteStatusReceived() {
  if (attempt_secure_connection_start_time_.is_null() ||
      attempt_get_remote_status_start_time_.is_null()) {
    PA_LOG(WARNING) << "Attempted to RecordUnlockableRemoteStatusReceived() "
                       "without initial timestamps recorded.";
    NOTREACHED();
  }

  base::Time now = base::DefaultClock::GetInstance()->Now();
  if (screenlock_type_ == ProximityAuthSystem::SESSION_LOCK) {
    // Use a custom |max| to account for Smart Lock's timeout (larger than the
    // default 10 seconds).
    base::UmaHistogramCustomTimes(
        "SmartLock.Performance.StartScanToReceiveUnlockableRemoteStatus."
        "Duration.Unlock",
        now - attempt_secure_connection_start_time_ /* sample */,
        kMinGetUnlockableRemoteStatusDuration /* min */,
        kMaxGetUnlockableRemoteStatusDuration /* max */,
        kNumDurationMetricBuckets /* buckets */);

    base::UmaHistogramTimes(
        "SmartLock.Performance.AuthenticationToReceiveUnlockableRemoteStatus."
        "Duration.Unlock",
        now - attempt_get_remote_status_start_time_);
  }

  // TODO(crbug.com/905438): Implement similar SignIn metrics.

  ResetPerformanceMetricsTimestamps();
}

void UnlockManagerImpl::ResetPerformanceMetricsTimestamps() {
  attempt_secure_connection_start_time_ = base::Time();
  attempt_get_remote_status_start_time_ = base::Time();
}

void UnlockManagerImpl::SetBluetoothSuspensionRecoveryTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  bluetooth_suspension_recovery_timer_ = std::move(timer);
}

}  // namespace proximity_auth
