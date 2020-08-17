// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {
namespace cellular_setup {

namespace {
mojom::ProfileState ProfileStateToMojo(int32_t state) {
  switch (state) {
    case hermes::profile::State::kActive:
      return mojom::ProfileState::kActive;
    case hermes::profile::State::kInactive:
      return mojom::ProfileState::kInactive;
    default:
      return mojom::ProfileState::kPending;
  }
}

mojom::ESimManager::ProfileInstallResult InstallResultFromStatus(
    HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      return ESimManager::ProfileInstallResult::kSuccess;
    case HermesResponseStatus::kErrorNeedConfirmationCode:
      return ESimManager::ProfileInstallResult::kErrorNeedsConfirmationCode;
    case HermesResponseStatus::kErrorInvalidActivationCode:
      return ESimManager::ProfileInstallResult::kErrorInvalidActivationCode;
    default:
      return ESimManager::ProfileInstallResult::kFailure;
  }
}

mojom::ESimManager::ESimOperationResult OperationResultFromStatus(
    HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      return ESimManager::ESimOperationResult::kSuccess;
    default:
      return ESimManager::ESimOperationResult::kFailure;
  }
}

}  // namespace

ESimManager::EuiccInfo::EuiccInfo(const dbus::ObjectPath& path,
                                  HermesEuiccClient::Properties* properties)
    : euicc_(mojom::Euicc::New()), path_(path) {
  CopyProperties(properties);
}

ESimManager::EuiccInfo::~EuiccInfo() = default;

bool ESimManager::EuiccInfo::ContainsIccid(const std::string& iccid) {
  return profile_iccids_.find(iccid) != profile_iccids_.end();
}

void ESimManager::EuiccInfo::CopyProperties(
    HermesEuiccClient::Properties* properties) {
  euicc_->eid = properties->eid().value();
  euicc_->is_active = properties->is_active().value();
}

ESimManager::ESimProfileInfo::ESimProfileInfo(
    const dbus::ObjectPath& path,
    const std::string& eid,
    HermesProfileClient::Properties* properties)
    : esim_profile_(mojom::ESimProfile::New()), path_(path) {
  CopyProperties(properties);
  esim_profile_->eid = eid;
}

ESimManager::ESimProfileInfo::~ESimProfileInfo() = default;

void ESimManager::ESimProfileInfo::CopyProperties(
    HermesProfileClient::Properties* properties) {
  esim_profile_->iccid = properties->iccid().value();
  esim_profile_->name = base::UTF8ToUTF16(properties->name().value());
  esim_profile_->nickname = base::UTF8ToUTF16(properties->nick_name().value());
  esim_profile_->service_provider =
      base::UTF8ToUTF16(properties->service_provider().value());
  esim_profile_->state = ProfileStateToMojo(properties->state().value());
  esim_profile_->activation_code = properties->activation_code().value();
}

ESimManager::ESimManager()
    : hermes_manager_client_(HermesManagerClient::Get()),
      hermes_euicc_client_(HermesEuiccClient::Get()),
      hermes_profile_client_(HermesProfileClient::Get()) {
  hermes_manager_client_->AddObserver(this);
  hermes_euicc_client_->AddObserver(this);
  hermes_profile_client_->AddObserver(this);
  UpdateAvailableEuiccs();
}

ESimManager::~ESimManager() {
  hermes_manager_client_->RemoveObserver(this);
  hermes_euicc_client_->RemoveObserver(this);
  hermes_profile_client_->RemoveObserver(this);
}

void ESimManager::BindReceiver(
    mojo::PendingReceiver<mojom::ESimManager> receiver) {
  NET_LOG(EVENT) << "ESimManager::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void ESimManager::AddObserver(
    mojo::PendingRemote<mojom::ESimManagerObserver> observer) {
  observers_.Add(std::move(observer));
}

void ESimManager::GetAvailableEuiccs(GetAvailableEuiccsCallback callback) {
  std::vector<mojom::EuiccPtr> euicc_list;
  for (auto const& eid : available_euicc_eids_)
    euicc_list.push_back(eid_euicc_info_map_[eid]->euicc()->Clone());
  std::move(callback).Run(std::move(euicc_list));
}

void ESimManager::GetProfiles(const std::string& eid,
                              GetProfilesCallback callback) {
  if (!eid_euicc_info_map_.count(eid)) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  EuiccInfo* euicc_info = eid_euicc_info_map_[eid].get();
  std::vector<mojom::ESimProfilePtr> profile_list;
  for (auto const& iccid : euicc_info->profile_iccids()) {
    profile_list.push_back(
        iccid_esim_profile_info_map_[iccid]->esim_profile()->Clone());
  }
  std::move(callback).Run(std::move(profile_list));
}

void ESimManager::RequestPendingProfiles(
    const std::string& eid,
    RequestPendingProfilesCallback callback) {
  if (!eid_euicc_info_map_.count(eid)) {
    NET_LOG(ERROR) << "RequestPendingProfiles failed: Unknown eid.";
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }
  NET_LOG(EVENT) << "Requesting Pending profiles";
  hermes_euicc_client_->RequestPendingEvents(
      eid_euicc_info_map_[eid]->path(),
      base::BindOnce(&ESimManager::OnRequestPendingEventsResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimManager::InstallProfileFromActivationCode(
    const std::string& eid,
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallProfileFromActivationCodeCallback callback) {
  ESimProfileInfo* profile_info = nullptr;
  ProfileInstallResult status = GetPendingProfileInfoFromActivationCode(
      activation_code, eid, &profile_info);
  if (profile_info &&
      status != mojom::ESimManager::ProfileInstallResult::kSuccess) {
    // Return early if profile was found but not in the correct state.
    std::move(callback).Run(status, nullptr);
    return;
  }

  if (profile_info) {
    CallInstallPendingProfile(profile_info, confirmation_code,
                              std::move(callback));
    return;
  }

  // Try installing directly with activation code.
  hermes_euicc_client_->InstallProfileFromActivationCode(
      eid_euicc_info_map_[eid]->path(), activation_code, confirmation_code,
      base::BindOnce(&ESimManager::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), eid));
}

void ESimManager::InstallProfile(const std::string& iccid,
                                 const std::string& confirmation_code,
                                 InstallProfileCallback callback) {
  ESimProfileInfo* profile_info = GetPendingProfileInfoFromIccid(iccid);
  if (!profile_info) {
    std::move(callback).Run(ProfileInstallResult::kFailure, nullptr);
    return;
  }

  CallInstallPendingProfile(profile_info, confirmation_code,
                            std::move(callback));
}

void ESimManager::UninstallProfile(const std::string& iccid,
                                   UninstallProfileCallback callback) {
  ESimProfileInfo* profile_info = GetInstalledProfileInfoFromIccid(iccid);
  if (!profile_info) {
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  EuiccInfo* euicc_info =
      eid_euicc_info_map_[profile_info->esim_profile()->eid].get();
  hermes_euicc_client_->UninstallProfile(
      euicc_info->path(), profile_info->path(),
      base::BindOnce(&ESimManager::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimManager::EnableProfile(const std::string& iccid,
                                EnableProfileCallback callback) {
  ESimProfileInfo* profile_info = GetInstalledProfileInfoFromIccid(iccid);
  if (!profile_info) {
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  if (profile_info->esim_profile()->state == mojom::ProfileState::kActive) {
    NET_LOG(ERROR) << "Profile enable failed: Profile already enabled";
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  hermes_profile_client_->EnableCarrierProfile(
      profile_info->path(),
      base::BindOnce(&ESimManager::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimManager::DisableProfile(const std::string& iccid,
                                 DisableProfileCallback callback) {
  ESimProfileInfo* profile_info = GetInstalledProfileInfoFromIccid(iccid);
  if (!profile_info) {
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  if (profile_info->esim_profile()->state == mojom::ProfileState::kInactive) {
    NET_LOG(ERROR) << "Profile enable failed: Profile already disabled";
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  hermes_profile_client_->DisableCarrierProfile(
      profile_info->path(),
      base::BindOnce(&ESimManager::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimManager::SetProfileNickname(const std::string& iccid,
                                     const base::string16& nickname,
                                     SetProfileNicknameCallback callback) {
  ESimProfileInfo* profile_info = GetInstalledProfileInfoFromIccid(iccid);
  if (!profile_info) {
    std::move(callback).Run(ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Properties* properties =
      hermes_profile_client_->GetProperties(profile_info->path());
  properties->nick_name().Set(
      base::UTF16ToUTF8(nickname),
      base::BindOnce(&ESimManager::OnProfilePropertySet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimManager::OnAvailableEuiccListChanged() {
  UpdateAvailableEuiccs();
  for (auto& observer : observers_)
    observer->OnAvailableEuiccListChanged();
}

void ESimManager::OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                                         const std::string& property_name) {
  EuiccInfo* euicc_info = GetEuiccInfoFromPath(euicc_path);
  // Skip notifying observers if the euicc object is not tracked.
  if (!euicc_info)
    return;
  if (property_name == hermes::euicc::kPendingProfilesProperty ||
      property_name == hermes::euicc::kInstalledProfilesProperty) {
    UpdateProfileList(euicc_info);
    for (auto& observer : observers_)
      observer->OnProfileListChanged(euicc_info->euicc()->eid);
  } else {
    HermesEuiccClient::Properties* properties =
        hermes_euicc_client_->GetProperties(euicc_path);
    euicc_info->CopyProperties(properties);
    for (auto& observer : observers_)
      observer->OnEuiccChanged(euicc_info->euicc()->Clone());
  }
}

void ESimManager::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& property_name) {
  ESimProfileInfo* profile_info = GetProfileInfoFromPath(carrier_profile_path);
  // Skip notifying observers if the carrier profile is not tracked.
  if (!profile_info)
    return;

  HermesProfileClient::Properties* properties =
      hermes_profile_client_->GetProperties(carrier_profile_path);
  profile_info->CopyProperties(properties);
  for (auto& observer : observers_)
    observer->OnProfileChanged(profile_info->esim_profile()->Clone());
}

void ESimManager::CallInstallPendingProfile(
    ESimProfileInfo* profile_info,
    const std::string& confirmation_code,
    ProfileInstallResultCallback callback) {
  mojom::ESimProfile* profile = profile_info->esim_profile();
  if (profile->state == mojom::ProfileState::kInstalling ||
      profile->state != mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Profile is already installed or in installing state.";
    std::move(callback).Run(ProfileInstallResult::kFailure, nullptr);
    return;
  }

  profile->state = mojom::ProfileState::kInstalling;
  for (auto& observer : observers_)
    observer->OnProfileChanged(profile->Clone());

  hermes_euicc_client_->InstallPendingProfile(
      eid_euicc_info_map_[profile->eid]->path(), profile_info->path(),
      confirmation_code,
      base::BindOnce(&ESimManager::OnPendingProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     profile_info));
}

void ESimManager::OnProfileInstallResult(ProfileInstallResultCallback callback,
                                         const std::string& eid,
                                         HermesResponseStatus status,
                                         const dbus::ObjectPath* object_path) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status="
                   << static_cast<int>(status);
    std::move(callback).Run(InstallResultFromStatus(status), nullptr);
    return;
  }

  HermesProfileClient::Properties* properties =
      hermes_profile_client_->GetProperties(*object_path);
  ESimProfileInfo* profile_info =
      GetOrCreateESimProfileInfo(properties, *object_path, eid);
  std::move(callback).Run(ProfileInstallResult::kSuccess,
                          profile_info->esim_profile()->Clone());
}

void ESimManager::OnPendingProfileInstallResult(
    ProfileInstallResultCallback callback,
    ESimProfileInfo* profile_info,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing pending profile status="
                   << static_cast<int>(status);
    profile_info->esim_profile()->state = mojom::ProfileState::kPending;
    for (auto& observer : observers_)
      observer->OnProfileChanged(profile_info->esim_profile()->Clone());
    std::move(callback).Run(InstallResultFromStatus(status), nullptr);
    return;
  }

  std::move(callback).Run(ProfileInstallResult::kSuccess,
                          profile_info->esim_profile()->Clone());
}

void ESimManager::OnESimOperationResult(ESimOperationResultCallback callback,
                                        HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESim operation error status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(OperationResultFromStatus(status));
}

void ESimManager::OnProfilePropertySet(ESimOperationResultCallback callback,
                                       bool success) {
  if (!success) {
    NET_LOG(ERROR) << "ESimProfile property set error.";
  }
  std::move(callback).Run(
      success ? mojom::ESimManager::ESimOperationResult::kSuccess
              : mojom::ESimManager::ESimOperationResult::kFailure);
}

void ESimManager::OnRequestPendingEventsResult(
    RequestPendingProfilesCallback callback,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Request Pending events failed status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(
      status == HermesResponseStatus::kSuccess
          ? mojom::ESimManager::ESimOperationResult::kSuccess
          : mojom::ESimManager::ESimOperationResult::kFailure);
}

void ESimManager::UpdateAvailableEuiccs() {
  NET_LOG(EVENT) << "Updating available Euiccs";
  available_euicc_eids_.clear();
  for (auto& euicc_path : hermes_manager_client_->GetAvailableEuiccs()) {
    HermesEuiccClient::Properties* properties =
        hermes_euicc_client_->GetProperties(euicc_path);
    EuiccInfo* euicc_info = GetOrCreateEuiccInfo(properties, euicc_path);
    available_euicc_eids_.insert(properties->eid().value());
    UpdateProfileList(euicc_info);
  }
  RemoveUntrackedEuiccs();
}

void ESimManager::UpdateProfileList(EuiccInfo* euicc_info) {
  HermesEuiccClient::Properties* euicc_properties =
      hermes_euicc_client_->GetProperties(euicc_info->path());
  std::set<std::string> profile_iccids;
  for (auto& path : euicc_properties->installed_carrier_profiles().value()) {
    HermesProfileClient::Properties* profile_properties =
        hermes_profile_client_->GetProperties(path);
    GetOrCreateESimProfileInfo(profile_properties, path,
                               euicc_info->euicc()->eid);
    profile_iccids.insert(profile_properties->iccid().value());
  }
  for (auto& path : euicc_properties->pending_carrier_profiles().value()) {
    HermesProfileClient::Properties* profile_properties =
        hermes_profile_client_->GetProperties(path);
    GetOrCreateESimProfileInfo(profile_properties, path,
                               euicc_info->euicc()->eid);
    profile_iccids.insert(profile_properties->iccid().value());
  }
  euicc_info->set_profile_iccids(profile_iccids);
  RemoveUntrackedProfiles(euicc_info);
}

void ESimManager::RemoveUntrackedEuiccs() {
  for (auto euicc_it = eid_euicc_info_map_.begin();
       euicc_it != eid_euicc_info_map_.end();) {
    const std::string& eid = euicc_it->first;
    if (available_euicc_eids_.find(eid) != available_euicc_eids_.end()) {
      euicc_it++;
      continue;
    }

    // Remove all profiles for this Euicc.
    for (auto esim_profile_it = iccid_esim_profile_info_map_.begin();
         esim_profile_it != iccid_esim_profile_info_map_.end();) {
      if (esim_profile_it->second->esim_profile()->eid == eid)
        esim_profile_it = iccid_esim_profile_info_map_.erase(esim_profile_it);
      else
        esim_profile_it++;
    }
    euicc_it = eid_euicc_info_map_.erase(euicc_it);
  }
}

void ESimManager::RemoveUntrackedProfiles(EuiccInfo* euicc_info) {
  for (auto it = iccid_esim_profile_info_map_.begin();
       it != iccid_esim_profile_info_map_.end();) {
    ESimProfileInfo* profile_info = it->second.get();
    const std::string& iccid = it->first;
    if (profile_info->esim_profile()->eid == euicc_info->euicc()->eid &&
        !euicc_info->ContainsIccid(iccid)) {
      it = iccid_esim_profile_info_map_.erase(it);
    } else {
      it++;
    }
  }
}

ESimManager::EuiccInfo* ESimManager::GetOrCreateEuiccInfo(
    HermesEuiccClient::Properties* properties,
    const dbus::ObjectPath& euicc_path) {
  EuiccInfo* euicc_info = GetEuiccInfoFromPath(euicc_path);
  if (euicc_info)
    return euicc_info;
  const std::string& eid = properties->eid().value();
  eid_euicc_info_map_[eid] =
      std::make_unique<EuiccInfo>(euicc_path, properties);
  return eid_euicc_info_map_[eid].get();
}

ESimManager::ESimProfileInfo* ESimManager::GetOrCreateESimProfileInfo(
    HermesProfileClient::Properties* properties,
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& eid) {
  ESimProfileInfo* profile_info = GetProfileInfoFromPath(carrier_profile_path);
  if (profile_info)
    return profile_info;
  const std::string& iccid = properties->iccid().value();
  iccid_esim_profile_info_map_[iccid] =
      std::make_unique<ESimProfileInfo>(carrier_profile_path, eid, properties);
  return iccid_esim_profile_info_map_[iccid].get();
}

ESimManager::EuiccInfo* ESimManager::GetEuiccInfoFromPath(
    const dbus::ObjectPath& path) {
  for (auto& pair : eid_euicc_info_map_) {
    if (pair.second->path() == path) {
      return eid_euicc_info_map_[pair.first].get();
    }
  }
  return nullptr;
}

ESimManager::ESimProfileInfo* ESimManager::GetProfileInfoFromPath(
    const dbus::ObjectPath& path) {
  for (auto& pair : iccid_esim_profile_info_map_) {
    if (pair.second->path() == path) {
      return pair.second.get();
    }
  }
  return nullptr;
}

mojom::ESimManager::ProfileInstallResult
ESimManager::GetPendingProfileInfoFromActivationCode(
    const std::string& eid,
    const std::string& activation_code,
    ESimProfileInfo** profile_info) {
  const auto iter = std::find_if(
      iccid_esim_profile_info_map_.begin(), iccid_esim_profile_info_map_.end(),
      [activation_code](const auto& pair) -> bool {
        return pair.second->esim_profile()->activation_code == activation_code;
      });
  if (iter == iccid_esim_profile_info_map_.end()) {
    NET_LOG(EVENT) << "Get pending profile with activation failed: No profile "
                      "with activation_code.";
    return ProfileInstallResult::kFailure;
  }
  *profile_info = iter->second.get();
  if (iter->second->esim_profile()->state != mojom::ProfileState::kPending ||
      iter->second->esim_profile()->eid != eid) {
    NET_LOG(ERROR) << "Get pending profile with activation code failed: Profile"
                      "is not in pending state or EID does not match.";
    return ProfileInstallResult::kFailure;
  }
  return ProfileInstallResult::kSuccess;
}

ESimManager::ESimProfileInfo* ESimManager::GetPendingProfileInfoFromIccid(
    const std::string& iccid) {
  if (!iccid_esim_profile_info_map_.count(iccid)) {
    NET_LOG(ERROR) << "Get pending profile with iccid failed: Unknown ICCID.";
    return nullptr;
  }
  mojom::ESimProfile* profile =
      iccid_esim_profile_info_map_[iccid]->esim_profile();
  if (profile->state != mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Get pending profile with iccid failed: Profile is "
                      "already installed or installing.";
    return nullptr;
  }
  return iccid_esim_profile_info_map_[iccid].get();
}

ESimManager::ESimProfileInfo* ESimManager::GetInstalledProfileInfoFromIccid(
    const std::string& iccid) {
  if (!iccid_esim_profile_info_map_.count(iccid)) {
    NET_LOG(ERROR) << "Get installed profile with iccid failed: Unknown iccid.";
    return nullptr;
  }

  mojom::ProfileState state =
      iccid_esim_profile_info_map_[iccid]->esim_profile()->state;
  if (state == mojom::ProfileState::kInstalling ||
      state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Get installed profile with iccid failed. Invalid state="
                   << state;
    return nullptr;
  }

  return iccid_esim_profile_info_map_[iccid].get();
}

}  // namespace cellular_setup
}  // namespace chromeos
