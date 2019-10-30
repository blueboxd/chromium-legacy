// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/sync_disable_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {

namespace {

enum DisableInfo {
  DISABLED_BY_NONE,
  // All commented values below were deprecated and removed.
  // DISABLED_BY_HISTORY,
  // DISABLED_BY_INITIALIZED,
  // DISABLED_BY_HISTORY_INITIALIZED,
  // DISABLED_BY_CONNECTED,
  // DISABLED_BY_HISTORY_CONNECTED,
  // DISABLED_BY_INITIALIZED_CONNECTED,
  // DISABLED_BY_HISTORY_INITIALIZED_CONNECTED,
  // DISABLED_BY_PASSPHRASE,
  // DISABLED_BY_HISTORY_PASSPHRASE,
  // DISABLED_BY_INITIALIZED_PASSPHRASE,
  // DISABLED_BY_HISTORY_INITIALIZED_PASSPHRASE,
  // DISABLED_BY_CONNECTED_PASSPHRASE,
  // DISABLED_BY_HISTORY_CONNECTED_PASSPHRASE,
  // DISABLED_BY_INITIALIZED_CONNECTED_PASSPHRASE,
  // DISABLED_BY_HISTORY_INITIALIZED_CONNECTED_PASSPHRASE,
  DISABLED_BY_ANONYMIZED_DATA_COLLECTION = 16,
  MAX_DISABLE_INFO
};

void RecordDisableInfo(DisableInfo info) {
  UMA_HISTOGRAM_ENUMERATION("UKM.SyncDisable.Info", info, MAX_DISABLE_INFO);
}

}  // namespace

SyncDisableObserver::SyncDisableObserver() : sync_observer_(this) {}

SyncDisableObserver::~SyncDisableObserver() {
  for (const auto& entry : consent_helpers_) {
    entry.second->RemoveObserver(this);
  }
}

bool SyncDisableObserver::SyncState::AllowsUkm() const {
  return anonymized_data_collection_enabled;
}

// static
SyncDisableObserver::SyncState SyncDisableObserver::GetSyncState(
    syncer::SyncService* sync_service,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(sync_service);
  DCHECK(consent_helper);
  SyncState state;
  state.anonymized_data_collection_enabled = consent_helper->IsEnabled();
  state.extensions_enabled =
      sync_service->GetPreferredDataTypes().Has(syncer::EXTENSIONS) &&
      sync_service->IsSyncFeatureEnabled();
  return state;
}

void SyncDisableObserver::ObserveServiceForSyncDisables(
    syncer::SyncService* sync_service,
    PrefService* prefs) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(prefs, sync_service);

  SyncState state = GetSyncState(sync_service, consent_helper.get());
  previous_states_[sync_service] = state;

  consent_helper->AddObserver(this);
  consent_helpers_[sync_service] = std::move(consent_helper);
  sync_observer_.Add(sync_service);
  UpdateAllProfileEnabled(false);
}

void SyncDisableObserver::UpdateAllProfileEnabled(bool must_purge) {
  bool all_sync_states_allow_ukm = CheckSyncStateOnAllProfiles();
  bool all_sync_states_allow_extension_ukm =
      all_sync_states_allow_ukm && CheckSyncStateForExtensionsOnAllProfiles();
  // Any change in sync settings needs to call OnSyncPrefsChanged so that the
  // new settings take effect.
  if (must_purge || (all_sync_states_allow_ukm != all_sync_states_allow_ukm_) ||
      (all_sync_states_allow_extension_ukm !=
       all_sync_states_allow_extension_ukm_)) {
    all_sync_states_allow_ukm_ = all_sync_states_allow_ukm;
    all_sync_states_allow_extension_ukm_ = all_sync_states_allow_extension_ukm;
    OnSyncPrefsChanged(must_purge);
  }
}

bool SyncDisableObserver::CheckSyncStateOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;

    if (state.AllowsUkm())
      continue;

    // Used as output for the disable reason UMA metrics.
    DCHECK(!state.anonymized_data_collection_enabled);
    RecordDisableInfo(DISABLED_BY_ANONYMIZED_DATA_COLLECTION);
    return false;
  }
  RecordDisableInfo(DISABLED_BY_NONE);
  return true;
}

bool SyncDisableObserver::CheckSyncStateForExtensionsOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;
    if (!state.extensions_enabled)
      return false;
  }
  return true;
}

void SyncDisableObserver::OnStateChanged(syncer::SyncService* sync) {
  UrlKeyedDataCollectionConsentHelper* consent_helper = nullptr;
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end())
    consent_helper = found->second.get();
  UpdateSyncState(sync, consent_helper);
}

void SyncDisableObserver::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(consent_helper);
  syncer::SyncService* sync_service = nullptr;
  for (const auto& entry : consent_helpers_) {
    if (consent_helper == entry.second.get()) {
      sync_service = entry.first;
      break;
    }
  }
  DCHECK(sync_service);
  UpdateSyncState(sync_service, consent_helper);
}

void SyncDisableObserver::UpdateSyncState(
    syncer::SyncService* sync,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(base::Contains(previous_states_, sync));
  const SyncDisableObserver::SyncState& previous_state = previous_states_[sync];
  DCHECK(consent_helper);
  SyncDisableObserver::SyncState state = GetSyncState(sync, consent_helper);

  // Trigger a purge if the current state no longer allows UKM.
  bool must_purge = previous_state.AllowsUkm() && !state.AllowsUkm();

  UMA_HISTOGRAM_BOOLEAN("UKM.SyncDisable.Purge", must_purge);

  previous_states_[sync] = state;
  UpdateAllProfileEnabled(must_purge);
}

void SyncDisableObserver::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK(base::Contains(previous_states_, sync));
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end()) {
    found->second->RemoveObserver(this);
    consent_helpers_.erase(found);
  }
  sync_observer_.Remove(sync);
  previous_states_.erase(sync);
  UpdateAllProfileEnabled(/*must_purge=*/false);
}

bool SyncDisableObserver::SyncStateAllowsUkm() {
  return all_sync_states_allow_ukm_;
}

bool SyncDisableObserver::SyncStateAllowsExtensionUkm() {
  return all_sync_states_allow_extension_ukm_;
}

}  // namespace ukm
