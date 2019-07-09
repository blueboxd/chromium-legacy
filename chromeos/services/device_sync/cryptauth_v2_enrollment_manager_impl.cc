// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/services/device_sync/cryptauth_v2_enroller_impl.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/public/cpp/client_app_metadata_provider.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Tune these values.
constexpr base::TimeDelta kWaitingForGcmRegistrationTimeout =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kWaitingForClientAppMetadataTimeout =
    base::TimeDelta::FromSeconds(10);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UserKeyPairState {
  // No v1 key; no v2 key. (Not enrolled)
  kNoV1KeyNoV2Key = 0,
  // v1 key exists; no v2 key. (Only v1 enrolled)
  kYesV1KeyNoV2Key = 1,
  // No v1 key; v2 key exists. (Only v2 enrolled)
  kNoV1KeyYesV2Key = 2,
  // v1 and v2 keys exist and agree.
  kYesV1KeyYesV2KeyAgree = 3,
  // v1 and v2 keys exist and disagree. (Enrolled with v2, rolled back to v1,
  // enrolled with v1, rolled forward to v2)
  kYesV1KeyYesV2KeyDisagree = 4,
  kMaxValue = kYesV1KeyYesV2KeyDisagree
};

cryptauthv2::ClientMetadata::InvocationReason ConvertInvocationReasonV1ToV2(
    cryptauth::InvocationReason invocation_reason_v1) {
  switch (invocation_reason_v1) {
    case cryptauth::InvocationReason::INVOCATION_REASON_UNKNOWN:
      return cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED;
    case cryptauth::InvocationReason::INVOCATION_REASON_INITIALIZATION:
      return cryptauthv2::ClientMetadata::INITIALIZATION;
    case cryptauth::InvocationReason::INVOCATION_REASON_PERIODIC:
      return cryptauthv2::ClientMetadata::PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_SLOW_PERIODIC:
      return cryptauthv2::ClientMetadata::SLOW_PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_FAST_PERIODIC:
      return cryptauthv2::ClientMetadata::FAST_PERIODIC;
    case cryptauth::InvocationReason::INVOCATION_REASON_EXPIRATION:
      return cryptauthv2::ClientMetadata::EXPIRATION;
    case cryptauth::InvocationReason::INVOCATION_REASON_FAILURE_RECOVERY:
      return cryptauthv2::ClientMetadata::FAILURE_RECOVERY;
    case cryptauth::InvocationReason::INVOCATION_REASON_NEW_ACCOUNT:
      return cryptauthv2::ClientMetadata::NEW_ACCOUNT;
    case cryptauth::InvocationReason::INVOCATION_REASON_CHANGED_ACCOUNT:
      return cryptauthv2::ClientMetadata::CHANGED_ACCOUNT;
    case cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED:
      return cryptauthv2::ClientMetadata::FEATURE_TOGGLED;
    case cryptauth::InvocationReason::INVOCATION_REASON_SERVER_INITIATED:
      return cryptauthv2::ClientMetadata::SERVER_INITIATED;
    case cryptauth::InvocationReason::INVOCATION_REASON_ADDRESS_CHANGE:
      return cryptauthv2::ClientMetadata::ADDRESS_CHANGE;
    case cryptauth::InvocationReason::INVOCATION_REASON_SOFTWARE_UPDATE:
      return cryptauthv2::ClientMetadata::SOFTWARE_UPDATE;
    case cryptauth::InvocationReason::INVOCATION_REASON_MANUAL:
      return cryptauthv2::ClientMetadata::MANUAL;
    default:
      PA_LOG(WARNING) << "Unknown v1 invocation reason: "
                      << invocation_reason_v1;
      return cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED;
  }
}

void RecordEnrollmentResult(CryptAuthEnrollmentResult result) {
  base::UmaHistogramBoolean("CryptAuth.EnrollmentV2.Result.Success",
                            result.IsSuccess());
  base::UmaHistogramEnumeration("CryptAuth.EnrollmentV2.Result.ResultCode",
                                result.result_code());
}

void RecordUserKeyPairState(const std::string& public_key_v1,
                            const std::string& private_key_v1,
                            const CryptAuthKey* key_v2) {
  bool v1_key_exists = !public_key_v1.empty() && !private_key_v1.empty();

  UserKeyPairState key_pair_state;
  if (v1_key_exists && key_v2) {
    if (public_key_v1 == key_v2->public_key() &&
        private_key_v1 == key_v2->private_key()) {
      key_pair_state = UserKeyPairState::kYesV1KeyYesV2KeyAgree;
    } else {
      key_pair_state = UserKeyPairState::kYesV1KeyYesV2KeyDisagree;
    }
  } else if (v1_key_exists && !key_v2) {
    key_pair_state = UserKeyPairState::kYesV1KeyNoV2Key;
  } else if (!v1_key_exists && key_v2) {
    key_pair_state = UserKeyPairState::kNoV1KeyYesV2Key;
  } else {
    key_pair_state = UserKeyPairState::kNoV1KeyNoV2Key;
  }

  base::UmaHistogramEnumeration("CryptAuth.EnrollmentV2.UserKeyPairState",
                                key_pair_state);
}

}  // namespace

// static
CryptAuthV2EnrollmentManagerImpl::Factory*
    CryptAuthV2EnrollmentManagerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthV2EnrollmentManagerImpl::Factory*
CryptAuthV2EnrollmentManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthV2EnrollmentManagerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthV2EnrollmentManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

// static
void CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // TODO(nohle): Remove when v1 Enrollment is deprecated.
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPublicKey,
                               std::string());
  registry->RegisterStringPref(prefs::kCryptAuthEnrollmentUserPrivateKey,
                               std::string());
}

// static
// Note: The enroller handles timeouts internally.
base::Optional<base::TimeDelta>
CryptAuthV2EnrollmentManagerImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForGcmRegistration:
      return kWaitingForGcmRegistrationTimeout;
    case State::kWaitingForClientAppMetadata:
      return kWaitingForClientAppMetadataTimeout;
    default:
      // Signifies that there should not be a timeout.
      return base::nullopt;
  }
}

// static
base::Optional<CryptAuthEnrollmentResult::ResultCode>
CryptAuthV2EnrollmentManagerImpl::ResultCodeErrorFromState(State state) {
  switch (state) {
    case State::kWaitingForGcmRegistration:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorTimeoutWaitingForGcmRegistration;
    case State::kWaitingForClientAppMetadata:
      return CryptAuthEnrollmentResult::ResultCode::
          kErrorTimeoutWaitingForClientAppMetadata;
    default:
      return base::nullopt;
  }
}

CryptAuthV2EnrollmentManagerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthEnrollmentManager>
CryptAuthV2EnrollmentManagerImpl::Factory::BuildInstance(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new CryptAuthV2EnrollmentManagerImpl(
      client_app_metadata_provider, key_registry, client_factory, gcm_manager,
      scheduler, pref_service, clock, std::move(timer)));
}

CryptAuthV2EnrollmentManagerImpl::CryptAuthV2EnrollmentManagerImpl(
    ClientAppMetadataProvider* client_app_metadata_provider,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_app_metadata_provider_(client_app_metadata_provider),
      key_registry_(key_registry),
      client_factory_(client_factory),
      gcm_manager_(gcm_manager),
      scheduler_(scheduler),
      pref_service_(pref_service),
      clock_(clock),
      timer_(std::move(timer)),
      callback_weak_ptr_factory_(this),
      scheduler_weak_ptr_factory_(this) {
  // TODO(nohle): Remove when v1 Enrollment is deprecated.
  AddV1UserKeyPairToRegistryIfNecessary();

  gcm_manager_->AddObserver(this);
}

CryptAuthV2EnrollmentManagerImpl::~CryptAuthV2EnrollmentManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthV2EnrollmentManagerImpl::Start() {
  scheduler_->StartEnrollmentScheduling(
      scheduler_weak_ptr_factory_.GetWeakPtr());

  // It is possible, though unlikely, that |scheduler_| has previously enrolled
  // successfully but |key_registry_| no longer holds the enrolled keys, for
  // example, if keys are deleted from the key registry or if the persisted key
  // registry pref cannot be parsed due to an encoding change. In this case,
  // force a re-enrollment.
  if (scheduler_->GetLastSuccessfulEnrollmentTime() &&
      (GetUserPublicKey().empty() || GetUserPrivateKey().empty())) {
    ForceEnrollmentNow(
        cryptauth::InvocationReason::INVOCATION_REASON_FAILURE_RECOVERY,
        base::nullopt /* session_id */);
  }
}

void CryptAuthV2EnrollmentManagerImpl::ForceEnrollmentNow(
    cryptauth::InvocationReason invocation_reason,
    const base::Optional<std::string>& session_id) {
  scheduler_->RequestEnrollment(
      ConvertInvocationReasonV1ToV2(invocation_reason), session_id);
}

bool CryptAuthV2EnrollmentManagerImpl::IsEnrollmentValid() const {
  base::Optional<base::Time> last_successful_enrollment_time =
      scheduler_->GetLastSuccessfulEnrollmentTime();

  if (!last_successful_enrollment_time)
    return false;

  if (GetUserPublicKey().empty() || GetUserPrivateKey().empty())
    return false;

  return (clock_->Now() - *last_successful_enrollment_time) <
         scheduler_->GetRefreshPeriod();
}

base::Time CryptAuthV2EnrollmentManagerImpl::GetLastEnrollmentTime() const {
  base::Optional<base::Time> last_successful_enrollment_time =
      scheduler_->GetLastSuccessfulEnrollmentTime();

  if (!last_successful_enrollment_time)
    return base::Time();

  return *last_successful_enrollment_time;
}

base::TimeDelta CryptAuthV2EnrollmentManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextEnrollmentRequest().value_or(
      base::TimeDelta::Max());
}

bool CryptAuthV2EnrollmentManagerImpl::IsEnrollmentInProgress() const {
  return state_ != State::kIdle;
}

bool CryptAuthV2EnrollmentManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetNumConsecutiveEnrollmentFailures() > 0;
}

std::string CryptAuthV2EnrollmentManagerImpl::GetUserPublicKey() const {
  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);

  // If a v1 key exists, it should have been added to the v2 registry already by
  // AddV1UserKeyPairToRegistryIfNecessary().
  DCHECK(
      GetV1UserPublicKey().empty() ||
      (user_key_pair && user_key_pair->public_key() == GetV1UserPublicKey()));

  if (!user_key_pair)
    return std::string();

  return user_key_pair->public_key();
}

std::string CryptAuthV2EnrollmentManagerImpl::GetUserPrivateKey() const {
  const CryptAuthKey* user_key_pair =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);
  std::string private_key_v1 = GetV1UserPrivateKey();

  // If a v1 key exists, it should have been added to the v2 registry already by
  // AddV1UserKeyPairToRegistryIfNecessary().
  DCHECK(
      GetV1UserPrivateKey().empty() ||
      (user_key_pair && user_key_pair->private_key() == GetV1UserPrivateKey()));

  if (!user_key_pair)
    return std::string();

  return user_key_pair->private_key();
}

void CryptAuthV2EnrollmentManagerImpl::OnEnrollmentRequested(
    const cryptauthv2::ClientMetadata& client_metadata,
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  DCHECK(state_ == State::kIdle);

  NotifyEnrollmentStarted();

  current_client_metadata_ = client_metadata;
  client_directive_policy_reference_ = client_directive_policy_reference;

  base::UmaHistogramExactLinear(
      "CryptAuth.EnrollmentV2.InvocationReason",
      current_client_metadata_->invocation_reason(),
      cryptauthv2::ClientMetadata::InvocationReason_ARRAYSIZE);

  AttemptEnrollment();
}

void CryptAuthV2EnrollmentManagerImpl::OnGCMRegistrationResult(bool success) {
  if (state_ != State::kWaitingForGcmRegistration)
    return;

  if (!success || gcm_manager_->GetRegistrationId().empty()) {
    OnEnrollmentFinished(CryptAuthEnrollmentResult(
        CryptAuthEnrollmentResult::ResultCode::kErrorGcmRegistrationFailed,
        base::nullopt /* client_directive */));
    return;
  }

  AttemptEnrollment();
}

void CryptAuthV2EnrollmentManagerImpl::OnReenrollMessage(
    const base::Optional<std::string>& session_id,
    const base::Optional<CryptAuthFeatureType>& feature_type) {
  ForceEnrollmentNow(cryptauth::INVOCATION_REASON_SERVER_INITIATED, session_id);
}

void CryptAuthV2EnrollmentManagerImpl::OnClientAppMetadataFetched(
    const base::Optional<cryptauthv2::ClientAppMetadata>& client_app_metadata) {
  DCHECK(state_ == State::kWaitingForClientAppMetadata);

  if (!client_app_metadata) {
    OnEnrollmentFinished(
        CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                      kErrorClientAppMetadataFetchFailed,
                                  base::nullopt /* client_directive */));
    return;
  }

  client_app_metadata_ = client_app_metadata;

  AttemptEnrollment();
}

void CryptAuthV2EnrollmentManagerImpl::AttemptEnrollment() {
  if (gcm_manager_->GetRegistrationId().empty()) {
    SetState(State::kWaitingForGcmRegistration);
    gcm_manager_->RegisterWithGCM();
    return;
  }

  if (!client_app_metadata_) {
    SetState(State::kWaitingForClientAppMetadata);
    client_app_metadata_provider_->GetClientAppMetadata(
        gcm_manager_->GetRegistrationId(),
        base::BindOnce(
            &CryptAuthV2EnrollmentManagerImpl::OnClientAppMetadataFetched,
            callback_weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  Enroll();
}

void CryptAuthV2EnrollmentManagerImpl::Enroll() {
  DCHECK(current_client_metadata_);
  DCHECK(client_app_metadata_);

  enroller_ = CryptAuthV2EnrollerImpl::Factory::Get()->BuildInstance(
      key_registry_, client_factory_);

  SetState(State::kWaitingForEnrollment);

  enroller_->Enroll(
      *current_client_metadata_, *client_app_metadata_,
      client_directive_policy_reference_,
      base::BindOnce(&CryptAuthV2EnrollmentManagerImpl::OnEnrollmentFinished,
                     callback_weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthV2EnrollmentManagerImpl::OnEnrollmentFinished(
    const CryptAuthEnrollmentResult& enrollment_result) {
  // Once an enrollment attempt finishes, no other callbacks should be
  // invoked. This is particularly relevant for timeout failures.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  enroller_.reset();

  if (enrollment_result.IsSuccess()) {
    PA_LOG(INFO) << "Enrollment attempt with invocation reason "
                 << current_client_metadata_->invocation_reason()
                 << " succeeded with result code "
                 << enrollment_result.result_code();
  } else {
    PA_LOG(WARNING) << "Enrollment attempt with invocation reason "
                    << current_client_metadata_->invocation_reason()
                    << " failed with result code "
                    << enrollment_result.result_code();
  }

  current_client_metadata_.reset();

  RecordEnrollmentResult(enrollment_result);

  scheduler_->HandleEnrollmentResult(enrollment_result);

  PA_LOG(INFO) << "Time until next enrollment attempt: "
               << GetTimeToNextAttempt();

  if (!enrollment_result.IsSuccess()) {
    PA_LOG(INFO) << "Number of consecutive Enrollment failures: "
                 << scheduler_->GetNumConsecutiveEnrollmentFailures();
  }

  SetState(State::kIdle);

  NotifyEnrollmentFinished(enrollment_result.IsSuccess());
}

void CryptAuthV2EnrollmentManagerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  base::Optional<CryptAuthEnrollmentResult::ResultCode> error_code =
      ResultCodeErrorFromState(state);

  // If there's a timeout specified, there should be a corresponding error
  // code.
  DCHECK(error_code);

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due to
  // async timeouts.
  timer_->Start(
      FROM_HERE, *timeout_for_state,
      base::BindOnce(&CryptAuthV2EnrollmentManagerImpl::OnEnrollmentFinished,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     CryptAuthEnrollmentResult(
                         *error_code, base::nullopt /*client_directive */)));
}

std::string CryptAuthV2EnrollmentManagerImpl::GetV1UserPublicKey() const {
  base::Optional<std::string> public_key = util::DecodeFromValueString(
      pref_service_->Get(prefs::kCryptAuthEnrollmentUserPublicKey));
  if (!public_key) {
    PA_LOG(ERROR) << "Invalid public key stored in user prefs.";
    return std::string();
  }

  return *public_key;
}

std::string CryptAuthV2EnrollmentManagerImpl::GetV1UserPrivateKey() const {
  base::Optional<std::string> private_key = util::DecodeFromValueString(
      pref_service_->Get(prefs::kCryptAuthEnrollmentUserPrivateKey));
  if (!private_key) {
    PA_LOG(ERROR) << "Invalid private key stored in user prefs.";
    return std::string();
  }

  return *private_key;
}

void CryptAuthV2EnrollmentManagerImpl::AddV1UserKeyPairToRegistryIfNecessary() {
  std::string public_key_v1 = GetV1UserPublicKey();
  std::string private_key_v1 = GetV1UserPrivateKey();
  const CryptAuthKey* key_v2 =
      key_registry_->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair);

  RecordUserKeyPairState(public_key_v1, private_key_v1, key_v2);

  // If the v1 user key pair does not exist, no action is needed.
  if (public_key_v1.empty() || private_key_v1.empty())
    return;

  // If the v1 and v2 user key pairs already agree, no action is needed.
  if (key_v2 && key_v2->public_key() == public_key_v1 &&
      key_v2->private_key() == private_key_v1) {
    return;
  }

  key_registry_->AddKey(
      CryptAuthKeyBundle::Name::kUserKeyPair,
      CryptAuthKey(public_key_v1, private_key_v1, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::P256,
                   kCryptAuthFixedUserKeyPairHandle));
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthV2EnrollmentManagerImpl::State& state) {
  switch (state) {
    case CryptAuthV2EnrollmentManagerImpl::State::kIdle:
      stream << "[EnrollmentManager state: Idle]";
      break;
    case CryptAuthV2EnrollmentManagerImpl::State::kWaitingForGcmRegistration:
      stream << "[EnrollmentManager state: Waiting for GCM registration]";
      break;
    case CryptAuthV2EnrollmentManagerImpl::State::kWaitingForClientAppMetadata:
      stream << "[EnrollmentManager state: Waiting for ClientAppMetadata]";
      break;
    case CryptAuthV2EnrollmentManagerImpl::State::kWaitingForEnrollment:
      stream << "[EnrollmentManager state: Waiting for enrollment to finish]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
