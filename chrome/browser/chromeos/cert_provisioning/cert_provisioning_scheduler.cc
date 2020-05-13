// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler.h"

#include <memory>
#include <unordered_map>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_metrics.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

template <typename Container, typename Value>
void EraseByKey(Container& container, const Value& value) {
  auto iter = container.find(value);
  if (iter == container.end()) {
    return;
  }

  container.erase(iter);
}

const base::TimeDelta kInconsistentDataErrorRetryDelay =
    base::TimeDelta::FromSeconds(30);

policy::CloudPolicyClient* GetCloudPolicyClientForDevice() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector) {
    return nullptr;
  }

  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (!policy_manager) {
    return nullptr;
  }

  policy::CloudPolicyCore* core = policy_manager->core();
  if (!core) {
    return nullptr;
  }

  return core->client();
}

policy::CloudPolicyClient* GetCloudPolicyClientForUser(Profile* profile) {
  policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (!user_cloud_policy_manager) {
    return nullptr;
  }

  policy::CloudPolicyCore* core = user_cloud_policy_manager->core();
  if (!core) {
    return nullptr;
  }

  return core->client();
}

NetworkStateHandler* GetNetworkStateHandler() {
  // Can happen in tests.
  if (!NetworkHandler::IsInitialized()) {
    return nullptr;
  }
  return NetworkHandler::Get()->network_state_handler();
}

}  // namespace

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningScheduler::CreateUserCertProvisioningScheduler(
    Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForUser(profile);
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!profile || !pref_service || !cloud_policy_client ||
      !network_state_handler) {
    LOG(ERROR) << "Failed to create user certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningScheduler>(
      CertScope::kUser, profile, pref_service,
      prefs::kRequiredClientCertificateForUser, cloud_policy_client,
      network_state_handler,
      std::make_unique<CertProvisioningUserInvalidatorFactory>(profile));
}

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningScheduler::CreateDeviceCertProvisioningScheduler(
    policy::AffiliatedInvalidationServiceProvider*
        invalidation_service_provider) {
  Profile* profile = ProfileHelper::GetSigninProfile();
  PrefService* pref_service = g_browser_process->local_state();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForDevice();
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!profile || !pref_service || !cloud_policy_client ||
      !network_state_handler) {
    LOG(ERROR) << "Failed to create device certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningScheduler>(
      CertScope::kDevice, profile, pref_service,
      prefs::kRequiredClientCertificateForDevice, cloud_policy_client,
      network_state_handler,
      std::make_unique<CertProvisioningDeviceInvalidatorFactory>(
          invalidation_service_provider));
}

CertProvisioningScheduler::CertProvisioningScheduler(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const char* pref_name,
    policy::CloudPolicyClient* cloud_policy_client,
    NetworkStateHandler* network_state_handler,
    std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory)
    : cert_scope_(cert_scope),
      profile_(profile),
      pref_service_(pref_service),
      pref_name_(pref_name),
      cloud_policy_client_(cloud_policy_client),
      network_state_handler_(network_state_handler),
      invalidator_factory_(std::move(invalidator_factory)) {
  CHECK(pref_service_);
  CHECK(pref_name_);
  CHECK(cloud_policy_client_);
  CHECK(profile);
  CHECK(invalidator_factory_);

  platform_keys_service_ =
      platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(profile);
  CHECK(platform_keys_service_);

  network_state_handler_->AddObserver(this, FROM_HERE);

  ScheduleInitialUpdate();
  ScheduleDailyUpdate();
}

CertProvisioningScheduler::~CertProvisioningScheduler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CertProvisioningScheduler::ScheduleInitialUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&CertProvisioningScheduler::InitialUpdateCerts,
                            weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::ScheduleDailyUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CertProvisioningScheduler::DailyUpdateCerts,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromDays(1));
}

void CertProvisioningScheduler::ScheduleRetry(const CertProfile& profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CertProvisioningScheduler::ProcessProfile,
                 weak_factory_.GetWeakPtr(), profile),
      kInconsistentDataErrorRetryDelay);
}

void CertProvisioningScheduler::InitialUpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DeleteCertsWithoutPolicy();
}

void CertProvisioningScheduler::DeleteCertsWithoutPolicy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<CertProfile> profiles = GetCertProfiles();
  std::set<std::string> cert_profile_ids_to_keep;
  for (const auto& profile : profiles) {
    cert_profile_ids_to_keep.insert(profile.profile_id);
  }

  cert_deleter_ = std::make_unique<CertProvisioningCertDeleter>();
  cert_deleter_->DeleteCerts(
      cert_scope_, platform_keys_service_, cert_profile_ids_to_keep,
      base::BindOnce(&CertProvisioningScheduler::OnDeleteCertsWithoutPolicyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::OnDeleteCertsWithoutPolicyDone(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  cert_deleter_.reset();

  if (!error_message.empty()) {
    LOG(ERROR) << "Failed to delete certificates without policies: "
               << error_message;
  }

  DeserializeWorkers();

  CleanVaKeysIfIdle();
}

void CertProvisioningScheduler::CleanVaKeysIfIdle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!workers_.empty()) {
    OnCleanVaKeysIfIdleDone(true);
    return;
  }

  DeleteVaKeysByPrefix(
      cert_scope_, profile_, kKeyNamePrefix,
      base::BindOnce(&CertProvisioningScheduler::OnCleanVaKeysIfIdleDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::OnCleanVaKeysIfIdleDone(
    base::Optional<bool> delete_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delete_result.has_value() || !delete_result.value()) {
    LOG(ERROR) << "Failed to delete keys while idle";
  }

  RegisterForPrefsChanges();
  UpdateCerts();
}

void CertProvisioningScheduler::RegisterForPrefsChanges() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_name_, base::BindRepeating(&CertProvisioningScheduler::OnPrefsChange,
                                      weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::DailyUpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  failed_cert_profiles_.clear();
  UpdateCerts();
  ScheduleDailyUpdate();
}

void CertProvisioningScheduler::DeserializeWorkers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* saved_workers =
      pref_service_->Get(GetPrefNameForSerialization(cert_scope_));
  if (!saved_workers) {
    return;
  }

  for (const auto& kv : saved_workers->DictItems()) {
    const base::Value& saved_worker = kv.second;

    std::unique_ptr<CertProvisioningWorker> worker =
        CertProvisioningWorkerFactory::Get()->Deserialize(
            cert_scope_, profile_, pref_service_, saved_worker,
            cloud_policy_client_, invalidator_factory_->Create(),
            base::BindOnce(&CertProvisioningScheduler::OnProfileFinished,
                           weak_factory_.GetWeakPtr()));
    if (!worker) {
      // Deserialization error message was already logged.
      continue;
    }

    workers_[worker->GetCertProfile().profile_id] = std::move(worker);
  }
}

void CertProvisioningScheduler::OnPrefsChange() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateCerts();
}

void CertProvisioningScheduler::UpdateOneCert(
    const std::string& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordEvent(cert_scope_, CertProvisioningEvent::kWorkerRetryManual);

  EraseByKey(failed_cert_profiles_, cert_profile_id);

  if (!CheckInternetConnection()) {
    WaitForInternetConnection();
    return;
  }

  base::Optional<CertProfile> cert_profile = GetOneCertProfile(cert_profile_id);
  if (!cert_profile) {
    return;
  }

  ProcessProfile(cert_profile.value());
}

void CertProvisioningScheduler::UpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_waiting_for_online_ = false;
  if (!CheckInternetConnection()) {
    WaitForInternetConnection();
    return;
  }

  if (certs_with_ids_getter_ && certs_with_ids_getter_->IsRunning()) {
    // Another UpdateCerts was started recently and still gethering info about
    // existing certs.
    return;
  }

  certs_with_ids_getter_ =
      std::make_unique<CertProvisioningCertsWithIdsGetter>();
  certs_with_ids_getter_->GetCertsWithIds(
      cert_scope_, platform_keys_service_,
      base::BindOnce(&CertProvisioningScheduler::OnGetCertsWithIdsDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::OnGetCertsWithIdsDone(
    std::map<std::string, scoped_refptr<net::X509Certificate>>
        existing_certs_with_ids,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  certs_with_ids_getter_.reset();

  if (!error_message.empty()) {
    LOG(ERROR) << "Failed to get existing cert ids: " << error_message;
    return;
  }

  std::vector<CertProfile> profiles = GetCertProfiles();

  CancelWorkersWithoutPolicy(profiles);

  for (const auto& profile : profiles) {
    if (base::Contains(existing_certs_with_ids, profile.profile_id) ||
        base::Contains(failed_cert_profiles_, profile.profile_id)) {
      continue;
    }

    ProcessProfile(profile);
  }
}

void CertProvisioningScheduler::ProcessProfile(
    const CertProfile& cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CertProvisioningWorker* worker = FindWorker(cert_profile.profile_id);
  if (!worker) {
    CreateCertProvisioningWorker(cert_profile);
    return;
  }

  if ((worker->GetCertProfile().policy_version !=
       cert_profile.policy_version)) {
    // The worker has outdated policy version. Make it stop, clean up current
    // state and report back through its callback. That will trigger retry for
    // its certificate profile.
    worker->Stop(CertProvisioningWorkerState::kInconsistentDataError);
    return;
  }

  if (worker->IsWaiting()) {
    worker->DoStep();
    return;
  }

  // There already is an active worker for this profile. No action required.
  return;
}

void CertProvisioningScheduler::CreateCertProvisioningWorker(
    CertProfile cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<CertProvisioningWorker> worker =
      CertProvisioningWorkerFactory::Get()->Create(
          cert_scope_, profile_, pref_service_, cert_profile,
          cloud_policy_client_, invalidator_factory_->Create(),
          base::BindOnce(&CertProvisioningScheduler::OnProfileFinished,
                         weak_factory_.GetWeakPtr()));
  CertProvisioningWorker* worker_unowned = worker.get();
  workers_[cert_profile.profile_id] = std::move(worker);
  worker_unowned->DoStep();
}

void CertProvisioningScheduler::OnProfileFinished(
    const CertProfile& profile,
    CertProvisioningWorkerState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto worker_iter = workers_.find(profile.profile_id);
  if (worker_iter == workers_.end()) {
    NOTREACHED();
    LOG(WARNING) << "Finished worker is not found";
    return;
  }

  switch (state) {
    case CertProvisioningWorkerState::kSucceeded:
      VLOG(0) << "Successfully provisioned certificate for profile: "
              << profile.profile_id;
      break;
    case CertProvisioningWorkerState::kInconsistentDataError:
      LOG(WARNING) << "Inconsistent data error for certificate profile: "
                   << profile.profile_id;
      ScheduleRetry(profile);
      break;
    case CertProvisioningWorkerState::kCanceled:
      break;
    default:
      LOG(ERROR) << "Failed to process certificate profile: "
                 << profile.profile_id;
      UpdateFailedCertProfiles(*(worker_iter->second));
      break;
  }

  workers_.erase(worker_iter);
}

CertProvisioningWorker* CertProvisioningScheduler::FindWorker(
    CertProfileId profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = workers_.find(profile_id);
  if (iter == workers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

base::Optional<CertProfile> CertProvisioningScheduler::GetOneCertProfile(
    const std::string& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* profile_list = pref_service_->Get(pref_name_);
  if (!profile_list) {
    LOG(WARNING) << "Preference is not found";
    return {};
  }

  for (const base::Value& cur_profile : profile_list->GetList()) {
    const std::string* id = cur_profile.FindStringKey(kCertProfileIdKey);
    if (!id || (*id != cert_profile_id)) {
      continue;
    }

    return CertProfile::MakeFromValue(cur_profile);
  }

  return base::nullopt;
}

std::vector<CertProfile> CertProvisioningScheduler::GetCertProfiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* profile_list = pref_service_->Get(pref_name_);
  if (!profile_list) {
    LOG(WARNING) << "Preference is not found";
    return {};
  }

  std::vector<CertProfile> result_profiles;
  for (const base::Value& cur_profile : profile_list->GetList()) {
    base::Optional<CertProfile> p = CertProfile::MakeFromValue(cur_profile);
    if (!p) {
      LOG(WARNING) << "Failed to parse certificate profile";
      continue;
    }

    result_profiles.emplace_back(std::move(p.value()));
  }

  return result_profiles;
}

const WorkerMap& CertProvisioningScheduler::GetWorkers() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return workers_;
}

const std::map<std::string, FailedWorkerInfo>&
CertProvisioningScheduler::GetFailedCertProfileIds() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return failed_cert_profiles_;
}

bool CertProvisioningScheduler::CheckInternetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const NetworkState* network = network_state_handler_->DefaultNetwork();
  return network && network->IsOnline();
}

void CertProvisioningScheduler::WaitForInternetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(0) << "Waiting for internet connection";
  is_waiting_for_online_ = true;
  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    worker_ptr->Pause();
  }
}

void CertProvisioningScheduler::OnNetworkChange(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_waiting_for_online_ && network && network->IsOnline()) {
    UpdateCerts();
    return;
  }

  if (!is_waiting_for_online_ && !workers_.empty() &&
      !CheckInternetConnection()) {
    WaitForInternetConnection();
    return;
  }
}

void CertProvisioningScheduler::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnNetworkChange(network);
}

void CertProvisioningScheduler::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnNetworkChange(network);
}

void CertProvisioningScheduler::UpdateFailedCertProfiles(
    const CertProvisioningWorker& worker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FailedWorkerInfo info;
  info.state = worker.GetPreviousState();
  info.public_key = worker.GetPublicKey();
  info.last_update_time = worker.GetLastUpdateTime();

  failed_cert_profiles_[worker.GetCertProfile().profile_id] = std::move(info);
}

void CertProvisioningScheduler::CancelWorkersWithoutPolicy(
    const std::vector<CertProfile>& profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (workers_.empty()) {
    return;
  }

  std::unordered_set<CertProfileId> cert_profile_ids;
  for (const CertProfile& profile : profiles) {
    cert_profile_ids.insert(profile.profile_id);
  }

  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    if (cert_profile_ids.find(worker_ptr->GetCertProfile().profile_id) ==
        cert_profile_ids.end()) {
      // This will trigger clean up (if any) in the worker and make it call its
      // callback.
      worker_ptr->Stop(CertProvisioningWorkerState::kCanceled);
    }
  }
}

}  // namespace cert_provisioning
}  // namespace chromeos
