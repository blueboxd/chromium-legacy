// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_
#define COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_

#include <map>

#include "base/scoped_observer.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

class PrefService;

namespace ukm {

// Observer that monitors whether UKM is allowed for all profiles.
//
// For one profile, UKM is allowed iff URL-keyed anonymized data collection is
// enabled.
class SyncDisableObserver
    : public syncer::SyncServiceObserver,
      public unified_consent::UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  SyncDisableObserver();
  ~SyncDisableObserver() override;

  // Starts observing a service for sync disables.
  void ObserveServiceForSyncDisables(syncer::SyncService* sync_service,
                                     PrefService* pref_service);

  // Returns true iff all sync states allow UKM to be enabled. This means that
  // URL-keyed anonymized data collection is enabled for all profiles.
  virtual bool SyncStateAllowsUkm();

  // Returns true iff sync is in a state that allows UKM to capture extensions.
  // This means that all profiles have EXTENSIONS data type enabled for syncing.
  virtual bool SyncStateAllowsExtensionUkm();

 protected:
  // Called after state changes and some profile has sync disabled.
  // If |must_purge| is true, sync was disabled for some profile, and
  // local data should be purged.
  virtual void OnSyncPrefsChanged(bool must_purge) = 0;

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // unified_consent::UrlKeyedDataCollectionConsentHelper::Observer:
  void OnUrlKeyedDataCollectionConsentStateChanged(
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper)
      override;

  // Recomputes all_profiles_enabled_ state from previous_states_;
  void UpdateAllProfileEnabled(bool must_purge);

  // Returns true iff all sync states in previous_states_ allow UKM.
  // If there are no profiles being observed, this returns false.
  bool CheckSyncStateOnAllProfiles();

  // Returns true iff all sync states in previous_states_ allow extension UKM.
  // If there are no profiles being observed, this returns false.
  bool CheckSyncStateForExtensionsOnAllProfiles();

  // Tracks observed history services, for cleanup.
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_;

  // State data about sync services that we need to remember.
  struct SyncState {
    // Returns true if this sync state allows UKM (i.e. URL-keyed anonymized
    // data collection is enabled).
    bool AllowsUkm() const;

    // Returns true if |AllowUkm| and if sync extensions are enabled.
    bool AllowsUkmWithExtension() const;

    // Whether anonymized data collection is enabled.
    bool anonymized_data_collection_enabled = false;

    // If the user has extension sync enabled.
    bool extensions_enabled = false;
  };

  // Updates the sync state for |sync| service. Updates all profiles if needed.
  // |sync| and |consent_helper| must not be null.
  void UpdateSyncState(
      syncer::SyncService* sync,
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper);

  // Gets the current state of a SyncService.
  // |sync| and |consent_helper| must not be null.
  static SyncState GetSyncState(
      syncer::SyncService* sync,
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper);

  // The state of the sync services being observed.
  std::map<syncer::SyncService*, SyncState> previous_states_;

  // The list of URL-keyed anonymized data collection consent helpers.
  //
  // Note: UrlKeyedDataCollectionConsentHelper does not rely on sync but there
  // must be exactly one per Chromium profile. As there is a single sync service
  // per profile, it is safe to key them by sync service instead of introducing
  // an additional map.
  std::map<
      syncer::SyncService*,
      std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>>
      consent_helpers_;

  // Tracks if UKM is allowed on all profiles after the last state change.
  bool all_sync_states_allow_ukm_ = false;

  // Tracks if extension sync was enabled on all profiles after the last state
  // change.
  bool all_sync_states_allow_extension_ukm_ = false;

  DISALLOW_COPY_AND_ASSIGN(SyncDisableObserver);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_
