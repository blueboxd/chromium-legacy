// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {
namespace {
const char kApplicationName[] = "com.google.chrome.fcm.invalidations";
// Sender ID coming from the Firebase console.
const char kInvalidationGCMSenderId[] = "8181035976";

void ReportInvalidatorState(syncer::InvalidatorState state) {
  UMA_HISTOGRAM_ENUMERATION("Invalidations.StatusChanged", state);
}

// Added in M76.
void MigratePrefs(PrefService* prefs, const std::string& sender_id) {
  if (!prefs->HasPrefPath(prefs::kFCMInvalidationClientIDCacheDeprecated)) {
    return;
  }
  DictionaryPrefUpdate update(prefs, prefs::kInvalidationClientIDCache);
  update->SetString(
      sender_id,
      prefs->GetString(prefs::kFCMInvalidationClientIDCacheDeprecated));
  prefs->ClearPref(prefs::kFCMInvalidationClientIDCacheDeprecated);
}

}  // namespace

FCMInvalidationService::FCMInvalidationService(
    IdentityProvider* identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const syncer::ParseJSONCallback& parse_json,
    network::mojom::URLLoaderFactory* loader_factory,
    const std::string& sender_id)
    : sender_id_(sender_id.empty() ? kInvalidationGCMSenderId : sender_id),
      invalidator_registrar_(pref_service,
                             sender_id_,
                             sender_id_ == kInvalidationGCMSenderId),
      gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      identity_provider_(identity_provider),
      pref_service_(pref_service),
      parse_json_(parse_json),
      loader_factory_(loader_factory),
      update_was_requested_(false) {}

FCMInvalidationService::~FCMInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.UpdateInvalidatorState(
      syncer::INVALIDATOR_SHUTTING_DOWN);
  identity_provider_->RemoveObserver(this);
  if (IsStarted())
    StopInvalidator();
}

void FCMInvalidationService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsReadyToStart()) {
    StartInvalidator();
  } else {
    if (identity_provider_->GetActiveAccountId().empty()) {
      ReportInvalidatorState(syncer::NOT_STARTED_NO_ACTIVE_ACCOUNT);
    } else {
      ReportInvalidatorState(syncer::NOT_STARTED_NO_REFRESH_TOKEN);
    }
  }

  identity_provider_->AddObserver(this);
}

// static
void FCMInvalidationService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      invalidation::prefs::kFCMInvalidationClientIDCacheDeprecated,
      /*default_value=*/std::string());
  registry->RegisterDictionaryPref(
      invalidation::prefs::kInvalidationClientIDCache);
}

void FCMInvalidationService::InitForTest(
    std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener) {
  // Here we perform the equivalent of Init() and StartInvalidator(), but with
  // some minor changes to account for the fact that we're injecting the
  // invalidation_listener.

  // StartInvalidator initializes the invalidation_listener and starts it.
  invalidation_listener_ = std::move(invalidation_listener);
  invalidation_listener_->StartForTest(this);

  DoUpdateRegisteredIdsIfNeeded();
}

void FCMInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_.RegisterHandler(handler);
  // Populate the id for newly registered handlers.
  handler->OnInvalidatorClientIdChange(client_id_);
  logger_.OnRegistration(handler->GetOwnerName());
}

bool FCMInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_was_requested_ = true;
  DVLOG(2) << "Registering ids: " << ids.size();
  syncer::Topics topics = ConvertIdsToTopics(ids, handler);
  if (!invalidator_registrar_.UpdateRegisteredTopics(handler, topics))
    return false;
  DoUpdateRegisteredIdsIfNeeded();
  logger_.OnUpdateTopics(invalidator_registrar_.GetSanitizedHandlersIdsMap());
  return true;
}

void FCMInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Unregistering";
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState FCMInvalidationService::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (invalidation_listener_) {
    DVLOG(2) << "GetInvalidatorState returning "
             << invalidator_registrar_.GetInvalidatorState();
    return invalidator_registrar_.GetInvalidatorState();
  }
  DVLOG(2) << "Invalidator currently stopped";
  return syncer::STOPPED;
}

std::string FCMInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

InvalidationLogger* FCMInvalidationService::GetInvalidationLogger() {
  return &logger_;
}

void FCMInvalidationService::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> return_callback)
    const {
  return_callback.Run(diagnostic_info_.CollectDebugData());
  invalidator_registrar_.RequestDetailedStatus(return_callback);
  if (identity_provider_) {
    identity_provider_->RequestDetailedStatus(return_callback);
  }
  if (IsStarted()) {
    invalidation_listener_->RequestDetailedStatus(return_callback);
  }
}

void FCMInvalidationService::OnActiveAccountLogin() {
  diagnostic_info_.active_account_login = base::Time::Now();
  diagnostic_info_.was_already_started_on_login = IsStarted();
  diagnostic_info_.was_ready_to_start_on_login = IsReadyToStart();
  diagnostic_info_.active_account_id = identity_provider_->GetActiveAccountId();

  if (IsStarted()) {
    return;
  }
  if (IsReadyToStart()) {
    StartInvalidator();
  } else {
    ReportInvalidatorState(syncer::NOT_STARTED_NO_REFRESH_TOKEN);
  }
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  diagnostic_info_.active_account_token_updated = base::Time::Now();
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountLogout() {
  diagnostic_info_.active_account_logged_out = base::Time::Now();
  diagnostic_info_.active_account_id = std::string();
  if (IsStarted()) {
    StopInvalidator();
    if (!client_id_.empty())
      ResetClientID();
  }
}

void FCMInvalidationService::OnInvalidate(
    const syncer::TopicInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);

  logger_.OnInvalidation(
      ConvertTopicInvalidationMapToObjectIdInvalidationMap(invalidation_map));
}

void FCMInvalidationService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  ReportInvalidatorState(state);
  invalidator_registrar_.UpdateInvalidatorState(state);
  logger_.OnStateChange(state);
}

bool FCMInvalidationService::IsReadyToStart() {
  bool valid_account_info_available =
      identity_provider_->IsActiveAccountWithRefreshToken();

#if defined(OS_ANDROID)
  // IsReadyToStart checks if account is available (active account logged in
  // and token is available). As currently observed, FCMInvalidationService
  // isn't always notified on Android when token is available.
  if (base::FeatureList::IsEnabled(
          invalidation::switches::
              kFCMInvalidationsStartOnceActiveAccountAvailable)) {
    valid_account_info_available =
        !identity_provider_->GetActiveAccountId().empty();
  }
#endif

  if (!valid_account_info_available) {
    DVLOG(2) << "Not starting FCMInvalidationService: "
             << "active account is not available";
    return false;
  }

  return true;
}

bool FCMInvalidationService::IsStarted() const {
  return invalidation_listener_ != nullptr;
}

void FCMInvalidationService::StartInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!invalidation_listener_);
  DCHECK(IsReadyToStart());
  diagnostic_info_.service_was_started = base::Time::Now();
  auto network = std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver_, instance_id_driver_, sender_id_, GetApplicationName());
  // The order of calls is important. Do not change.
  // We should start listening before requesting the id, because
  // valid id is only generated, once there is an app handler
  // for the app. StartListening registers the app handler.
  // We should create InvalidationListener first, because it registers the
  // handler for the incoming messages, which is crutial on Android, because on
  // the startup cached messages might exists.
  invalidation_listener_ =
      std::make_unique<syncer::FCMInvalidationListener>(std::move(network));
  auto registration_manager =
      std::make_unique<syncer::PerUserTopicRegistrationManager>(
          identity_provider_, pref_service_, loader_factory_, parse_json_,
          sender_id_, sender_id_ == kInvalidationGCMSenderId);
  invalidation_listener_->Start(this, std::move(registration_manager));

  PopulateClientID();
  DoUpdateRegisteredIdsIfNeeded();
}

void FCMInvalidationService::StopInvalidator() {
  DCHECK(invalidation_listener_);
  diagnostic_info_.service_was_stopped = base::Time::Now();
  // TODO(melandory): reset the network.
  invalidation_listener_.reset();
}

void FCMInvalidationService::PopulateClientID() {
  diagnostic_info_.instance_id_requested = base::Time::Now();
  if (sender_id_ == kInvalidationGCMSenderId) {
    MigratePrefs(pref_service_, sender_id_);
  }
  const std::string* client_id_pref =
      pref_service_->GetDictionary(prefs::kInvalidationClientIDCache)
          ->FindStringKey(sender_id_);
  client_id_ = client_id_pref ? *client_id_pref : "";
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->GetID(base::Bind(&FCMInvalidationService::OnInstanceIdRecieved,
                                base::Unretained(this)));
}

void FCMInvalidationService::ResetClientID() {
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->DeleteID(base::Bind(&FCMInvalidationService::OnDeleteIDCompleted,
                                   base::Unretained(this)));
  DictionaryPrefUpdate update(pref_service_, prefs::kInvalidationClientIDCache);
  update->RemoveKey(sender_id_);
}

void FCMInvalidationService::OnInstanceIdRecieved(const std::string& id) {
  diagnostic_info_.instance_id_received = base::Time::Now();
  if (client_id_ != id) {
    client_id_ = id;
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kInvalidationClientIDCache);
    update->SetStringKey(sender_id_, id);
    invalidator_registrar_.UpdateInvalidatorId(id);
  }
}

void FCMInvalidationService::OnDeleteIDCompleted(
    instance_id::InstanceID::Result) {
  // TODO(meandory): report metric in case of unsucesfull deletion.
}

void FCMInvalidationService::DoUpdateRegisteredIdsIfNeeded() {
  if (!invalidation_listener_ || !update_was_requested_)
    return;
  auto registered_ids = invalidator_registrar_.GetAllRegisteredIds();
  invalidation_listener_->UpdateRegisteredTopics(registered_ids);
  update_was_requested_ = false;
}

const std::string FCMInvalidationService::GetApplicationName() {
  // If using the default |sender_id_|, use the bare |kApplicationName|, so the
  // old app name is maintained.
  if (sender_id_ == kInvalidationGCMSenderId) {
    return kApplicationName;
  }
  return base::StrCat({kApplicationName, "-", sender_id_});
}

FCMInvalidationService::Diagnostics::Diagnostics() {}

base::DictionaryValue FCMInvalidationService::Diagnostics::CollectDebugData()
    const {
  base::DictionaryValue status;

  status.SetString("InvalidationService.Active-account-login",
                   base::TimeFormatShortDateAndTime(active_account_login));
  status.SetString(
      "InvalidationService.Active-account-token-updated",
      base::TimeFormatShortDateAndTime(active_account_token_updated));
  status.SetString("InvalidationService.Active-account-logged-out",
                   base::TimeFormatShortDateAndTime(active_account_logged_out));
  status.SetString("InvalidationService.IID-requested",
                   base::TimeFormatShortDateAndTime(instance_id_requested));
  status.SetString("InvalidationService.IID-received",
                   base::TimeFormatShortDateAndTime(instance_id_received));
  status.SetString("InvalidationService.Service-stopped",
                   base::TimeFormatShortDateAndTime(service_was_stopped));
  status.SetString("InvalidationService.Service-started",
                   base::TimeFormatShortDateAndTime(service_was_started));
  status.SetBoolean("InvalidationService.Started-on-active-account-login",
                    was_already_started_on_login);
  status.SetBoolean(
      "InvalidationService.Ready-to-start-on-active-account-login",
      was_ready_to_start_on_login);
  status.SetString("InvalidationService.Active-account-id", active_account_id);
  return status;
}

}  // namespace invalidation
