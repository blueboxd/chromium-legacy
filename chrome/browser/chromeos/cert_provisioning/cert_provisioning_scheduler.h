// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
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

struct FailedWorkerInfo {
  CertProvisioningWorkerState state = CertProvisioningWorkerState::kInitState;
  std::string public_key;
};

// This class is a part of certificate provisioning feature. It tracks updates
// of |RequiredClientCertificateForUser|, |RequiredClientCertificateForDevice|
// policies and creates one CertProvisioningWorker for every policy entry.
class CertProvisioningScheduler : public NetworkStateHandlerObserver {
 public:
  static std::unique_ptr<CertProvisioningScheduler>
  CreateUserCertProvisioningScheduler(Profile* profile);
  static std::unique_ptr<CertProvisioningScheduler>
  CreateDeviceCertProvisioningScheduler();

  CertProvisioningScheduler(CertScope cert_scope,
                            Profile* profile,
                            PrefService* pref_service,
                            const char* pref_name,
                            policy::CloudPolicyClient* cloud_policy_client,
                            NetworkStateHandler* network_state_handler);
  ~CertProvisioningScheduler() override;

  CertProvisioningScheduler(const CertProvisioningScheduler&) = delete;
  CertProvisioningScheduler& operator=(const CertProvisioningScheduler&) =
      delete;

  void UpdateOneCert(const std::string& cert_profile_id);
  void UpdateCerts();
  void OnProfileFinished(const CertProfile& profile,
                         CertProvisioningWorkerState state);

  const WorkerMap& GetWorkers() const;
  const std::map<std::string, FailedWorkerInfo>& GetFailedCertProfileIds()
      const;

 private:
  void ScheduleInitialUpdate();
  void ScheduleDailyUpdate();
  // Posts delayed task to call ProcessProfile.
  void ScheduleRetry(const CertProfile& profile);

  void InitialUpdateCerts();
  void DeleteCertsWithoutPolicy();
  void OnDeleteKeysWithoutPolicyDone(const std::string& error_message);

  void OnPrefsChange();
  void DailyUpdateCerts();
  void DeserializeWorkers();

  void OnGetCertsWithIdsDone(
      std::map<std::string, scoped_refptr<net::X509Certificate>>
          existing_certs_with_ids,
      const std::string& error_message);

  // Creates a new worker for |profile| if there is no at the moment.
  // Recreates a worker if existing one has a different version of the profile.
  // Continues an existing worker if it is in a waiting state.
  void ProcessProfile(const CertProfile& profile);

  base::Optional<CertProfile> GetOneCertProfile(
      const std::string& cert_profile_id);
  std::vector<CertProfile> GetCertProfiles();

  void CreateCertProvisioningWorker(CertProfile profile);
  CertProvisioningWorker* FindWorker(CertProfileId profile_id);

  bool CheckInternetConnection();
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
  NetworkStateHandler* network_state_handler_ = nullptr;
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  WorkerMap workers_;
  // Collection of cert profile ids that failed recently. They will not be
  // retried until next |DailyUpdateCerts|. FailedWorkerInfo contains some extra
  // information about the failure. Profiles that failed with
  // kInconsistentDataError will not be stored into this collection.
  std::map<std::string /*cert_profile_id*/, FailedWorkerInfo>
      failed_cert_profiles_;
  // Equals true if the last attempt to update certificates failed because there
  // was no internet connection.
  bool is_waiting_for_online_ = false;

  std::unique_ptr<CertProvisioningCertsWithIdsGetter> certs_with_ids_getter_;
  std::unique_ptr<CertProvisioningCertDeleter> cert_deleter_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningScheduler> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
