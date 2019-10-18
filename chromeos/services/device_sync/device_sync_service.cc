// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/device_sync_service.h"

#include "base/bind.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/device_sync_base.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace device_sync {

DeviceSyncService::DeviceSyncService(
    signin::IdentityManager* identity_manager,
    gcm::GCMDriver* gcm_driver,
    const GcmDeviceInfoProvider* gcm_device_info_provider,
    ClientAppMetadataProvider* client_app_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingReceiver<mojom::DeviceSyncServiceInitializer> init_receiver)
    : init_receiver_(this, std::move(init_receiver)),
      identity_manager_(identity_manager),
      gcm_driver_(gcm_driver),
      gcm_device_info_provider_(gcm_device_info_provider),
      client_app_metadata_provider_(client_app_metadata_provider),
      url_loader_factory_(std::move(url_loader_factory)) {}

DeviceSyncService::~DeviceSyncService() {
  // Subclasses may hold onto message response callbacks. It's important that
  // all bindings are closed by the time those callbacks are destroyed, or they
  // will DCHECK.
  if (device_sync_)
    device_sync_->CloseAllBindings();
}

void DeviceSyncService::Initialize(
    mojo::PendingReceiver<mojom::DeviceSyncService> receiver,
    mojo::PendingRemote<prefs::mojom::PrefStoreConnector>
        pref_store_connector) {
  PA_LOG(VERBOSE) << "DeviceSyncService::Init()";
  receiver_.Bind(std::move(receiver));
  device_sync_ = DeviceSyncImpl::Factory::Get()->BuildInstance(
      identity_manager_, gcm_driver_, std::move(pref_store_connector),
      gcm_device_info_provider_, client_app_metadata_provider_,
      url_loader_factory_, std::make_unique<base::OneShotTimer>());
}

void DeviceSyncService::BindDeviceSync(
    mojo::PendingReceiver<mojom::DeviceSync> receiver) {
  PA_LOG(VERBOSE) << "DeviceSyncService::BindDeviceSync()";
  device_sync_->BindRequest(std::move(receiver));
}

}  // namespace device_sync

}  // namespace chromeos
