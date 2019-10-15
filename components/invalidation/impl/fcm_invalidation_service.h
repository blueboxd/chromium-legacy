// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/backoff_entry.h"

class PrefService;
class PrefRegistrySimple;

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMNetworkHandler;
class PerUserTopicRegistrationManager;
}  // namespace syncer

namespace invalidation {

using FCMNetworkHandlerCallback =
    base::RepeatingCallback<std::unique_ptr<syncer::FCMNetworkHandler>(
        const std::string& sender_id,
        const std::string& app_id)>;

using PerUserTopicRegistrationManagerCallback =
    base::RepeatingCallback<std::unique_ptr<
        syncer::PerUserTopicRegistrationManager>(const std::string& project_id,
                                                 bool migrate_prefs)>;

// This InvalidationService wraps the C++ Invalidation Client (FCM) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class FCMInvalidationService
    : public InvalidationService,
      public IdentityProvider::Observer,
      public syncer::FCMInvalidationListener::Delegate {
 public:
  FCMInvalidationService(IdentityProvider* identity_provider,
                         FCMNetworkHandlerCallback fcm_network_handler_callback,
                         PerUserTopicRegistrationManagerCallback
                             per_user_topic_registration_manager_callback,
                         instance_id::InstanceIDDriver* client_id_driver,
                         PrefService* pref_service,
                         const std::string& sender_id = {});
  ~FCMInvalidationService() override;

  void Init();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> caller)
      const override;

  // IdentityProvider::Observer implementation.
  void OnActiveAccountRefreshTokenUpdated() override;
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // syncer::FCMInvalidationListener::Delegate implementation.
  void OnInvalidate(
      const syncer::TopicInvalidationMap& invalidation_map) override;
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;

 protected:
  friend class FCMInvalidationServiceTestDelegate;

  // Initializes with an injected invalidator.
  void InitForTest(
      std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener);

 private:
  struct Diagnostics {
    Diagnostics();

    // Collect all the internal variables in a single readable dictionary.
    base::DictionaryValue CollectDebugData() const;

    base::Time active_account_login;
    base::Time active_account_token_updated;
    base::Time active_account_logged_out;
    base::Time instance_id_requested;
    base::Time instance_id_received;
    base::Time service_was_stopped;
    base::Time service_was_started;
    bool was_already_started_on_login = false;
    bool was_ready_to_start_on_login = false;
    CoreAccountId active_account_id;
  };

  bool IsReadyToStart();
  bool IsStarted() const;

  void StartInvalidator();
  void StopInvalidator();

  void PopulateClientID();
  void ResetClientID();
  void OnInstanceIdRecieved(const std::string& id);
  void OnDeleteIDCompleted(instance_id::InstanceID::Result);

  void DoUpdateRegisteredIdsIfNeeded();

  const std::string GetApplicationName();

  const std::string sender_id_;

  syncer::InvalidatorRegistrarWithMemory invalidator_registrar_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  FCMNetworkHandlerCallback fcm_network_handler_callback_;
  PerUserTopicRegistrationManagerCallback
      per_user_topic_registration_manager_callback_;

  instance_id::InstanceIDDriver* const instance_id_driver_;
  std::string client_id_;

  IdentityProvider* const identity_provider_;
  PrefService* const pref_service_;

  bool update_was_requested_ = false;
  Diagnostics diagnostic_info_;

  // The invalidation listener.
  std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidationService);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
