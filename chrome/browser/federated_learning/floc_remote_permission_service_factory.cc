// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_remote_permission_service_factory.h"

#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {
// Returns true if the user is signed in and full history sync is enabled,
// and false otherwise.
bool IsHistorySyncEnabled(Profile* profile) {
  syncer::SyncService* sync = ProfileSyncServiceFactory::GetForProfile(profile);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace

// static
FlocRemotePermissionServiceFactory*
FlocRemotePermissionServiceFactory::GetInstance() {
  return base::Singleton<FlocRemotePermissionServiceFactory>::get();
}

// static
federated_learning::FlocRemotePermissionService*
FlocRemotePermissionServiceFactory::GetForProfile(Profile* profile) {
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return static_cast<federated_learning::FlocRemotePermissionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* FlocRemotePermissionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return new federated_learning::FlocRemotePermissionService(
      IdentityManagerFactory::GetForProfile(profile),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

FlocRemotePermissionServiceFactory::FlocRemotePermissionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FlocRemotePermissionServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

FlocRemotePermissionServiceFactory::~FlocRemotePermissionServiceFactory() =
    default;
