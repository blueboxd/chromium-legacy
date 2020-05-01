// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler.h"
#include <memory>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace cert_provisioning {

namespace {
const char kCertProfileIdKey[] = "cert_profile_id";

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

}  // namespace

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningScheduler::CreateUserCertProvisioningScheduler(
    Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForUser(profile);

  if (!profile || !pref_service || !cloud_policy_client) {
    LOG(ERROR) << "Failed to create user certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningScheduler>(
      CertScope::kUser, profile, pref_service,
      prefs::kRequiredClientCertificateForUser, cloud_policy_client);
}

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningScheduler::CreateDeviceCertProvisioningScheduler() {
  Profile* profile = ProfileHelper::GetSigninProfile();
  PrefService* pref_service = g_browser_process->local_state();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForDevice();

  if (!profile || !pref_service || !cloud_policy_client) {
    LOG(ERROR) << "Failed to create device certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningScheduler>(
      CertScope::kDevice, profile, pref_service,
      prefs::kRequiredClientCertificateForDevice, cloud_policy_client);
}

CertProvisioningScheduler::CertProvisioningScheduler(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const char* pref_name,
    policy::CloudPolicyClient* cloud_policy_client)
    : cert_scope_(cert_scope),
      profile_(profile),
      pref_service_(pref_service),
      pref_name_(pref_name),
      cloud_policy_client_(cloud_policy_client) {
  CHECK(pref_service_);
  CHECK(pref_name_);
  CHECK(cloud_policy_client_);
  CHECK(profile);

  platform_keys_service_ =
      platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(profile);
  CHECK(platform_keys_service_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_name_, base::BindRepeating(&CertProvisioningScheduler::OnPrefsChange,
                                      weak_factory_.GetWeakPtr()));

  ScheduleInitialUpdate();
  ScheduleDailyUpdate();
}

CertProvisioningScheduler::~CertProvisioningScheduler() = default;

void CertProvisioningScheduler::ScheduleInitialUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&CertProvisioningScheduler::InitialUpdateCerts,
                            weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::ScheduleDailyUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CertProvisioningScheduler::DailyUpdateCerts,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromDays(1));
}

void CertProvisioningScheduler::InitialUpdateCerts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeleteCertsWithoutPolicy();
}

void CertProvisioningScheduler::DeleteCertsWithoutPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CertProfile> profiles = GetCertProfiles();
  std::set<std::string> cert_profile_ids_to_keep;
  for (const auto& profile : profiles) {
    cert_profile_ids_to_keep.insert(profile.profile_id);
  }

  cert_deleter_ = std::make_unique<CertProvisioningCertDeleter>();
  cert_deleter_->DeleteCerts(
      cert_scope_, platform_keys_service_, cert_profile_ids_to_keep,
      base::BindOnce(&CertProvisioningScheduler::OnDeleteKeysWithoutPolicyDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningScheduler::OnDeleteKeysWithoutPolicyDone(
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cert_deleter_.reset();

  if (!error_message.empty()) {
    LOG(ERROR) << "Failed to delete certificates without policies: "
               << error_message;
  }

  DeserializeWorkers();
  UpdateCerts();
}

void CertProvisioningScheduler::DailyUpdateCerts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  failed_cert_profiles_.clear();
  UpdateCerts();
  ScheduleDailyUpdate();
}

void CertProvisioningScheduler::DeserializeWorkers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
            cloud_policy_client_,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateCerts();
}

void CertProvisioningScheduler::UpdateCerts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  certs_with_ids_getter_.reset();

  if (!error_message.empty()) {
    LOG(ERROR) << "Failed to get existing cert ids: " << error_message;
    return;
  }

  std::vector<CertProfile> profiles = GetCertProfiles();
  if (profiles.empty()) {
    workers_.clear();
    return;
  }

  for (const auto& profile : profiles) {
    if (base::Contains(existing_certs_with_ids, profile.profile_id) ||
        base::Contains(failed_cert_profiles_, profile.profile_id)) {
      continue;
    }

    ProcessProfile(profile);
  }
}

void CertProvisioningScheduler::ProcessProfile(const CertProfile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CertProvisioningWorker* worker = FindWorker(profile.profile_id);
  if (!worker) {
    CreateCertProvisioningWorker(profile);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<CertProvisioningWorker> worker =
      CertProvisioningWorkerFactory::Get()->Create(
          cert_scope_, profile_, pref_service_, cert_profile,
          cloud_policy_client_,
          base::BindOnce(&CertProvisioningScheduler::OnProfileFinished,
                         weak_factory_.GetWeakPtr()));
  CertProvisioningWorker* worker_unowned = worker.get();
  workers_[cert_profile.profile_id] = std::move(worker);
  worker_unowned->DoStep();
}

void CertProvisioningScheduler::OnProfileFinished(
    const CertProfile& cert_profile,
    bool is_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_success) {
    LOG(ERROR) << "Failed to process certificate profile: "
               << cert_profile.profile_id;
    failed_cert_profiles_.insert(cert_profile.profile_id);
  }

  auto iter = workers_.find(cert_profile.profile_id);
  if (iter == workers_.end()) {
    LOG(WARNING) << "Finished worker is not found: " << cert_profile.profile_id;
    return;
  }
  workers_.erase(iter);
}

CertProvisioningWorker* CertProvisioningScheduler::FindWorker(
    CertProfileId profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = workers_.find(profile_id);
  if (iter == workers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

std::vector<CertProfile> CertProvisioningScheduler::GetCertProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* profile_list = pref_service_->Get(pref_name_);
  if (!profile_list) {
    LOG(WARNING) << "Preference is not found";
    return {};
  }

  std::vector<CertProfile> result_profiles;
  for (const base::Value& cur_profile : profile_list->GetList()) {
    const std::string* id = cur_profile.FindStringKey(kCertProfileIdKey);
    if (!id) {
      LOG(WARNING) << "Id field is not found";
      continue;
    }

    result_profiles.emplace_back();
    result_profiles.back().profile_id = *id;
  }

  return result_profiles;
}

size_t CertProvisioningScheduler::GetWorkerCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return workers_.size();
}

const std::set<std::string>&
CertProvisioningScheduler::GetFailedCertProfilesIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return failed_cert_profiles_;
}

}  // namespace cert_provisioning
}  // namespace chromeos
