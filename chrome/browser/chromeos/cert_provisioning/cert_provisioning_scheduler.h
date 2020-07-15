// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_platform_keys_helpers.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class PrefService;

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class NetworkStateHandler;

namespace platform_keys {
class PlatformKeysService;
}  // namespace platform_keys

namespace cert_provisioning {

class CertProvisioningWorker;

using WorkerMap =
    std::map<CertProfileId, std::unique_ptr<CertProvisioningWorker>>;

using CertProfileSet = base::flat_set<CertProfile, CertProfileComparator>;

// Holds information about a worker which failed that is still useful (e.g. for
// UI) after the worker has been destroyed.
struct FailedWorkerInfo {
  // The state the worker had prior to switching to the failed state
  // (CertProvisioningWorkerState::kFailed).
  CertProvisioningWorkerState state_before_failure =
      CertProvisioningWorkerState::kInitState;
  // The DER-encoded X.509 SPKI.
  std::string public_key;
  // The time the worker was last updated, i.e. when it transferred to the
  // failed state.
  base::Time last_update_time;
};

// This class is a part of certificate provisioning feature. It tracks updates
// of |RequiredClientCertificateForUser|, |RequiredClientCertificateForDevice|
// policies and creates one CertProvisioningWorker for every policy entry.
// Should work on the UI thread because it interacts with PlatformKeysService
// and some methods are called from the UI to populate certificate manager
// settings page.
class CertProvisioningScheduler : public NetworkStateHandlerObserver {
 public:
  static std::unique_ptr<CertProvisioningScheduler>
  CreateUserCertProvisioningScheduler(Profile* profile);
  static std::unique_ptr<CertProvisioningScheduler>
  CreateDeviceCertProvisioningScheduler(
      policy::AffiliatedInvalidationServiceProvider*
          invalidation_service_provider);

  CertProvisioningScheduler(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      policy::CloudPolicyClient* cloud_policy_client,
      platform_keys::PlatformKeysService* platform_keys_service,
      NetworkStateHandler* network_state_handler,
      std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory);
  ~CertProvisioningScheduler() override;

  CertProvisioningScheduler(const CertProvisioningScheduler&) = delete;
  CertProvisioningScheduler& operator=(const CertProvisioningScheduler&) =
      delete;
  // Intended to be called when a user presses a button in certificate manager
  // UI. Retries provisioning of a specific certificate.
  void UpdateOneCert(const CertProfileId& cert_profile_id);
  void UpdateAllCerts();
  void OnProfileFinished(const CertProfile& profile,
                         CertProvisioningWorkerState state);

  const WorkerMap& GetWorkers() const;

  const base::flat_map<CertProfileId, FailedWorkerInfo>&
  GetFailedCertProfileIds() const;

 private:
  void ScheduleInitialUpdate();
  void ScheduleDailyUpdate();
  // Posts delayed task to call UpdateOneCertImpl.
  void ScheduleRetry(const CertProfileId& profile_id);
  void ScheduleRenewal(const CertProfileId& profile_id, base::TimeDelta delay);

  void InitialUpdateCerts();
  void DeleteCertsWithoutPolicy();
  void OnDeleteCertsWithoutPolicyDone(const std::string& error_message);
  void CancelWorkersWithoutPolicy(const std::vector<CertProfile>& profiles);
  void CleanVaKeysIfIdle();
  void OnCleanVaKeysIfIdleDone(base::Optional<bool> delete_result);
  void RegisterForPrefsChanges();

  void InitiateRenewal(const CertProfileId& cert_profile_id);
  void UpdateOneCertImpl(const CertProfileId& cert_profile_id);
  void UpdateCertList(std::vector<CertProfile> profiles);
  void UpdateCertListWithExistingCerts(
      std::vector<CertProfile> profiles,
      base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
          existing_certs_with_ids,
      const std::string& error_message);

  void OnPrefsChange();
  void DailyUpdateCerts();
  void DeserializeWorkers();

  // Creates a new worker for |profile| if there is no at the moment.
  // Recreates a worker if existing one has a different version of the profile.
  // Continues an existing worker if it is in a waiting state.
  void ProcessProfile(const CertProfile& profile);

  base::Optional<CertProfile> GetOneCertProfile(
      const CertProfileId& cert_profile_id);
  std::vector<CertProfile> GetCertProfiles();

  void CreateCertProvisioningWorker(CertProfile profile);
  CertProvisioningWorker* FindWorker(CertProfileId profile_id);

  // Returns true if the process can be continued (if it's not required to
  // wait).
  bool MaybeWaitForInternetConnection();
  void WaitForInternetConnection();
  void OnNetworkChange(const NetworkState* network);
  // NetworkStateHandlerObserver
  void DefaultNetworkChanged(const NetworkState* network) override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  void UpdateFailedCertProfiles(const CertProvisioningWorker& worker);

  CertScope cert_scope_ = CertScope::kUser;
  Profile* profile_ = nullptr;
  PrefService* pref_service_ = nullptr;
  const char* pref_name_ = nullptr;
  policy::CloudPolicyClient* cloud_policy_client_ = nullptr;
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  WorkerMap workers_;
  // Contains cert profile ids that will be renewed before next daily update.
  // Helps to prevent creation of more than one delayed task for renewal. When
  // the renewal starts for a profile id, it is removed from the set.
  base::flat_set<CertProfileId> scheduled_renewals_;
  // Collection of cert profile ids that failed recently. They will not be
  // retried until next |DailyUpdateCerts|. FailedWorkerInfo contains some extra
  // information about the failure. Profiles that failed with
  // kInconsistentDataError will not be stored into this collection.
  base::flat_map<CertProfileId, FailedWorkerInfo> failed_cert_profiles_;
  // Equals true if the last attempt to update certificates failed because there
  // was no internet connection.
  bool is_waiting_for_online_ = false;

  // Contains profiles that should be updated after the current update batch
  // run, because an update for them was triggered during the current run.
  CertProfileSet queued_profiles_to_update_;

  LatestCertsWithIdsGetter certs_with_ids_getter_;
  CertDeleter cert_deleter_;
  std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory_;

  base::WeakPtrFactory<CertProvisioningScheduler> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
