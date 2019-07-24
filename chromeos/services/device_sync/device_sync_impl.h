// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/device_sync_base.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/services/device_sync/remote_device_provider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "services/preferences/public/cpp/pref_service_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

class PrefService;

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthDeviceManager;
class CryptAuthScheduler;
class CryptAuthKeyRegistry;
class GcmDeviceInfoProvider;
class SoftwareFeatureManager;

// Concrete DeviceSync implementation. When DeviceSyncImpl is constructed, it
// starts an initialization flow with the following steps:
// (1) Verify that the primary user is logged in with a valid account ID.
// (2) Connect to the Prefs Service associated with that account.
// (3) Instantiate classes which communicate with the CryptAuth back-end.
// (4) Check enrollment state; if not yet enrolled, enroll the device.
// (5) When enrollment is valid, listen for device sync updates.
class DeviceSyncImpl : public DeviceSyncBase,
                       public CryptAuthEnrollmentManager::Observer,
                       public RemoteDeviceProvider::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* test_factory);

    virtual ~Factory();

    // Note: |timer| should be a newly-created base::OneShotTimer object; this
    // parameter only exists for testing via dependency injection.
    virtual std::unique_ptr<DeviceSyncBase> BuildInstance(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        service_manager::Connector* connector,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer);

   private:
    static Factory* test_factory_instance_;
  };

  ~DeviceSyncImpl() override;

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentFinished(bool success) override;

  // RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override;

 private:
  friend class DeviceSyncServiceTest;

  enum class Status {
    FETCHING_ACCOUNT_INFO,
    CONNECTING_TO_USER_PREFS,
    WAITING_FOR_ENROLLMENT,
    READY
  };

  // Wrapper around preferences code. This class is necessary so that tests can
  // override this functionality to use a fake PrefService rather than a real
  // connection to the Preferences service.
  class PrefConnectionDelegate {
   public:
    virtual ~PrefConnectionDelegate();

    virtual scoped_refptr<PrefRegistrySimple> CreatePrefRegistry();
    virtual void ConnectToPrefService(
        service_manager::Connector* connector,
        scoped_refptr<PrefRegistrySimple> pref_registry,
        prefs::ConnectCallback callback);
  };

  class PendingSetSoftwareFeatureRequest {
   public:
    PendingSetSoftwareFeatureRequest(
        const std::string& device_public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        RemoteDeviceProvider* remote_device_provider,
        SetSoftwareFeatureStateCallback callback);
    ~PendingSetSoftwareFeatureRequest();

    // Whether the request has been fulfilled (i.e., whether the requested
    // feature has been set according to the parameters used).
    bool IsFulfilled() const;

    void InvokeCallback(mojom::NetworkRequestResult result);

    multidevice::SoftwareFeature software_feature() const {
      return software_feature_;
    }

    bool enabled() const { return enabled_; }

   private:
    std::string device_public_key_;
    multidevice::SoftwareFeature software_feature_;
    bool enabled_;
    RemoteDeviceProvider* remote_device_provider_;
    SetSoftwareFeatureStateCallback callback_;
  };

  DeviceSyncImpl(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      service_manager::Connector* connector,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Clock* clock,
      std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate,
      std::unique_ptr<base::OneShotTimer> timer);

  // DeviceSyncBase:
  void Shutdown() override;

  void ProcessPrimaryAccountInfo(const CoreAccountInfo& primary_account_info);
  void ConnectToPrefStore();
  void OnConnectedToPrefService(std::unique_ptr<PrefService> pref_service);
  void InitializeCryptAuthManagementObjects();
  void CompleteInitializationAfterSuccessfulEnrollment();

  base::Optional<multidevice::RemoteDevice> GetSyncedDeviceWithPublicKey(
      const std::string& public_key) const;

  void OnSetSoftwareFeatureStateSuccess();
  void OnSetSoftwareFeatureStateError(const base::UnguessableToken& request_id,
                                      NetworkRequestError error);
  void OnFindEligibleDevicesSuccess(
      const base::RepeatingCallback<
          void(mojom::NetworkRequestResult,
               mojom::FindEligibleDevicesResponsePtr)>& callback,
      const std::vector<cryptauth::ExternalDeviceInfo>& eligible_devices,
      const std::vector<cryptauth::IneligibleDevice>& ineligible_devices);
  void OnFindEligibleDevicesError(
      const base::RepeatingCallback<
          void(mojom::NetworkRequestResult,
               mojom::FindEligibleDevicesResponsePtr)>& callback,
      NetworkRequestError error);

  // Note: If the timer is already running, StartSetSoftwareFeatureTimer()
  // restarts it.
  void StartSetSoftwareFeatureTimer();
  void OnSetSoftwareFeatureTimerFired();

  void SetPrefConnectionDelegateForTesting(
      std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate);

  signin::IdentityManager* identity_manager_;
  gcm::GCMDriver* gcm_driver_;
  service_manager::Connector* connector_;
  const GcmDeviceInfoProvider* gcm_device_info_provider_;
  ClientAppMetadataProvider* client_app_metadata_provider_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::Clock* clock_;
  std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate_;
  std::unique_ptr<base::OneShotTimer> set_software_feature_timer_;

  Status status_;
  CoreAccountInfo primary_account_info_;
  std::unique_ptr<PrefService> pref_service_;
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<PendingSetSoftwareFeatureRequest>>
      id_to_pending_set_software_feature_request_map_;

  std::unique_ptr<CryptAuthGCMManager> cryptauth_gcm_manager_;
  std::unique_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // Only created and used if v2 Enrollment is enabled; null otherwise.
  std::unique_ptr<CryptAuthKeyRegistry> cryptauth_key_registry_;
  std::unique_ptr<CryptAuthScheduler> cryptauth_scheduler_;

  std::unique_ptr<CryptAuthEnrollmentManager> cryptauth_enrollment_manager_;
  std::unique_ptr<CryptAuthDeviceManager> cryptauth_device_manager_;
  std::unique_ptr<RemoteDeviceProvider> remote_device_provider_;
  std::unique_ptr<SoftwareFeatureManager> software_feature_manager_;

  base::WeakPtrFactory<DeviceSyncImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
