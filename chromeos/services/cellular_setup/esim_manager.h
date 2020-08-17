// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_MANAGER_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace cellular_setup {

// Implementation of mojom::ESimManager. This class uses the Hermes
// DBus clients to communicate with the Hermes daemon and provide
// eSIM management methods. ESimManager mojo interface is provided
// in WebUI for cellular settings and eSIM setup.
class ESimManager : public mojom::ESimManager,
                    HermesManagerClient::Observer,
                    HermesEuiccClient::Observer,
                    HermesProfileClient::Observer {
 public:
  ESimManager();
  ESimManager(const ESimManager&) = delete;
  ESimManager& operator=(const ESimManager&) = delete;
  ~ESimManager() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ESimManager> receiver);

  // mojom::ESimManager
  void AddObserver(
      mojo::PendingRemote<mojom::ESimManagerObserver> observer) override;
  void GetAvailableEuiccs(GetAvailableEuiccsCallback callback) override;
  void GetProfiles(const std::string& eid,
                   GetProfilesCallback callback) override;
  void RequestPendingProfiles(const std::string& eid,
                              RequestPendingProfilesCallback callback) override;
  void InstallProfileFromActivationCode(
      const std::string& eid,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallProfileFromActivationCodeCallback callback) override;
  void InstallProfile(const std::string& iccid,
                      const std::string& confirmation_code,
                      InstallProfileCallback callback) override;
  void UninstallProfile(const std::string& iccid,
                        UninstallProfileCallback callback) override;
  void EnableProfile(const std::string& iccid,
                     EnableProfileCallback callback) override;
  void DisableProfile(const std::string& iccid,
                      DisableProfileCallback callback) override;
  void SetProfileNickname(const std::string& iccid,
                          const base::string16& nickname,
                          SetProfileNicknameCallback callback) override;

  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // HermesEuiccClient::Observer:
  void OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                              const std::string& property_name) override;

  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& property_name) override;

 private:
  // EuiccInfo object is used to track state of Hermes Euicc objects.
  // It holds mojom::Euicc object along with other related values.
  class EuiccInfo {
   public:
    EuiccInfo(const dbus::ObjectPath& path,
              HermesEuiccClient::Properties* properties);
    ~EuiccInfo();

    // Returns a boolean indicating whether |iccid| exists
    // in installed or pending profiles for this Euicc.
    bool ContainsIccid(const std::string& iccid);
    void CopyProperties(HermesEuiccClient::Properties* properties);
    mojom::Euicc* euicc() { return euicc_.get(); }
    const dbus::ObjectPath& path() { return path_; }
    void set_profile_iccids(const std::set<std::string>& profile_iccids) {
      profile_iccids_ = std::move(profile_iccids);
    }
    const std::set<std::string>& profile_iccids() { return profile_iccids_; }

   private:
    mojom::EuiccPtr euicc_;
    dbus::ObjectPath path_;
    std::set<std::string> profile_iccids_;
  };

  // ESimProfileInfo is used to track state of Hermes Profile objects.
  // It holds mojom::ESimProfile object along with other related values.
  class ESimProfileInfo {
   public:
    ESimProfileInfo(const dbus::ObjectPath& path,
                    const std::string& eid,
                    HermesProfileClient::Properties* properties);
    ~ESimProfileInfo();

    void CopyProperties(HermesProfileClient::Properties* properties);
    mojom::ESimProfile* esim_profile() { return esim_profile_.get(); }
    const dbus::ObjectPath& path() { return path_; }

   private:
    mojom::ESimProfilePtr esim_profile_;
    dbus::ObjectPath path_;
  };

  // Type of callback for profile installation methods.
  using ProfileInstallResultCallback =
      base::OnceCallback<void(mojom::ESimManager::ProfileInstallResult,
                              mojom::ESimProfilePtr)>;
  // Type of callback for other esim manager methods.
  using ESimOperationResultCallback =
      base::OnceCallback<void(mojom::ESimManager::ESimOperationResult)>;

  void CallInstallPendingProfile(ESimProfileInfo* profile_info,
                                 const std::string& confirmation_code,
                                 ProfileInstallResultCallback callback);
  void OnRequestPendingEventsResult(RequestPendingProfilesCallback callback,
                                    HermesResponseStatus status);
  void OnPendingProfileInstallResult(ProfileInstallResultCallback callback,
                                     ESimProfileInfo* profile_info,
                                     HermesResponseStatus status);
  void OnProfileInstallResult(ProfileInstallResultCallback callback,
                              const std::string& eid,
                              HermesResponseStatus status,
                              const dbus::ObjectPath* object_path);
  void OnESimOperationResult(ESimOperationResultCallback callback,
                             HermesResponseStatus status);
  void OnProfilePropertySet(ESimOperationResultCallback callback, bool success);
  void UpdateAvailableEuiccs();
  void UpdateProfileList(EuiccInfo* euicc_info);
  void RemoveUntrackedEuiccs();
  void RemoveUntrackedProfiles(EuiccInfo* euicc_info);
  EuiccInfo* GetOrCreateEuiccInfo(HermesEuiccClient::Properties* properties,
                                  const dbus::ObjectPath& euicc_path);
  ESimProfileInfo* GetOrCreateESimProfileInfo(
      HermesProfileClient::Properties* properties,
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& eid);
  EuiccInfo* GetEuiccInfoFromPath(const dbus::ObjectPath& path);
  ESimProfileInfo* GetProfileInfoFromPath(const dbus::ObjectPath& path);
  ProfileInstallResult GetPendingProfileInfoFromActivationCode(
      const std::string& eid,
      const std::string& activation_code,
      ESimProfileInfo** profile_info);
  ESimProfileInfo* GetPendingProfileInfoFromIccid(const std::string& iccid);
  ESimProfileInfo* GetInstalledProfileInfoFromIccid(const std::string& iccid);

  HermesManagerClient* hermes_manager_client_;
  HermesEuiccClient* hermes_euicc_client_;
  HermesProfileClient* hermes_profile_client_;
  std::set<std::string> available_euicc_eids_;
  std::map<std::string, std::unique_ptr<EuiccInfo>> eid_euicc_info_map_;
  std::map<std::string, std::unique_ptr<ESimProfileInfo>>
      iccid_esim_profile_info_map_;
  mojo::RemoteSet<mojom::ESimManagerObserver> observers_;
  mojo::ReceiverSet<mojom::ESimManager> receivers_;

  base::WeakPtrFactory<ESimManager> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_MANAGER_H_
