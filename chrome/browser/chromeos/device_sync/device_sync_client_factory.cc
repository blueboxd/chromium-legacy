// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service_factory.h"
#include "chrome/browser/chromeos/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/device_sync_service.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace device_sync {

namespace {

// CryptAuth enrollment is allowed only if at least one multi-device feature is
// enabled. This ensures that we do not unnecessarily register devices on the
// CryptAuth back-end when the registration would never actually be used.
bool IsEnrollmentAllowedByPolicy(content::BrowserContext* context) {
  return multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
      Profile::FromBrowserContext(context)->GetPrefs());
}

std::unique_ptr<DeviceSyncService> CreateServiceInstanceForProfile(
    Profile* profile,
    mojo::Remote<chromeos::device_sync::mojom::DeviceSyncService>* remote) {
  mojo::Remote<chromeos::device_sync::mojom::DeviceSyncServiceInitializer>
      initializer;
  auto service = std::make_unique<DeviceSyncService>(
      IdentityManagerFactory::GetForProfile(profile),
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
      chromeos::GcmDeviceInfoProviderImpl::GetInstance(),
      chromeos::ClientAppMetadataProviderServiceFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(), initializer.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<prefs::mojom::PrefStoreConnector> pref_store_connector;
  content::BrowserContext::GetConnectorFor(profile)->Connect(
      prefs::mojom::kServiceName,
      pref_store_connector.InitWithNewPipeAndPassReceiver());
  initializer->Initialize(remote->BindNewPipeAndPassReceiver(),
                          std::move(pref_store_connector));
  return service;
}

}  // namespace

// Class that wraps DeviceSyncClient in a KeyedService.
class DeviceSyncClientHolder : public KeyedService {
 public:
  explicit DeviceSyncClientHolder(content::BrowserContext* context)
      : service_(CreateServiceInstanceForProfile(
            Profile::FromBrowserContext(context),
            &remote_service_)),
        device_sync_client_(DeviceSyncClientImpl::Factory::Get()->BuildInstance(
            remote_service_.get())) {}

  DeviceSyncClient* device_sync_client() { return device_sync_client_.get(); }

 private:
  // KeyedService:
  void Shutdown() override { device_sync_client_.reset(); }

  mojo::Remote<chromeos::device_sync::mojom::DeviceSyncService> remote_service_;

  // The in-process service instance. Never exposed publicly except through the
  // DeviceSyncClient, which is isolated from the service by Mojo interfaces.
  std::unique_ptr<chromeos::device_sync::DeviceSyncService> service_;

  std::unique_ptr<DeviceSyncClient> device_sync_client_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClientHolder);
};

DeviceSyncClientFactory::DeviceSyncClientFactory()
    : BrowserContextKeyedServiceFactory(
          "DeviceSyncClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

DeviceSyncClientFactory::~DeviceSyncClientFactory() {}

// static
DeviceSyncClient* DeviceSyncClientFactory::GetForProfile(Profile* profile) {
  DeviceSyncClientHolder* holder = static_cast<DeviceSyncClientHolder*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  return holder ? holder->device_sync_client() : nullptr;
}

// static
DeviceSyncClientFactory* DeviceSyncClientFactory::GetInstance() {
  return base::Singleton<DeviceSyncClientFactory>::get();
}

KeyedService* DeviceSyncClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(crbug.com/848347): Check prohibited by policy in services that depend
  // on this Factory, not here.
  if (IsEnrollmentAllowedByPolicy(context))
    return new DeviceSyncClientHolder(context);

  return nullptr;
}

bool DeviceSyncClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace device_sync

}  // namespace chromeos
