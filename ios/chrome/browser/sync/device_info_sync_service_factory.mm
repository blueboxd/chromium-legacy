// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_sync_service_impl.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
syncer::DeviceInfoSyncService* DeviceInfoSyncServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
DeviceInfoSyncServiceFactory* DeviceInfoSyncServiceFactory::GetInstance() {
  return base::Singleton<DeviceInfoSyncServiceFactory>::get();
}

// static
void DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers(
    std::vector<const syncer::DeviceInfoTracker*>* trackers) {
  DCHECK(trackers);
  std::vector<ios::ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (ios::ChromeBrowserState* browser_state : browser_state_list) {
    syncer::DeviceInfoSyncService* service =
        DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);
    if (service != nullptr) {
      const syncer::DeviceInfoTracker* tracker =
          service->GetDeviceInfoTracker();
      if (tracker != nullptr) {
        trackers->push_back(tracker);
      }
    }
  }
}

DeviceInfoSyncServiceFactory::DeviceInfoSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DeviceInfoSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

DeviceInfoSyncServiceFactory::~DeviceInfoSyncServiceFactory() {}

std::unique_ptr<KeyedService>
DeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          ::GetChannel(), ::GetVersionString(),
          /*signin_scoped_device_id_callback=*/
          base::BindRepeating(&signin::GetSigninScopedDeviceId,
                              browser_state->GetPrefs()),
          /*send_tab_to_self_receiving_enabled_callback=*/
          base::BindRepeating(
              &send_tab_to_self::IsReceivingEnabledByUserOnThisDevice,
              browser_state->GetPrefs()));
  auto device_prefs =
      std::make_unique<syncer::DeviceInfoPrefs>(browser_state->GetPrefs());

  return std::make_unique<syncer::DeviceInfoSyncServiceImpl>(
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs));
}
