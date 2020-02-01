// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

IsolatedPrerenderServiceWorkersObserver::
    IsolatedPrerenderServiceWorkersObserver(Profile* profile) {
  // For testing.
  if (!profile)
    return;

  // Only the default storage partition is supported for Isolated Prerender.
  content::ServiceWorkerContext* service_worker_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetServiceWorkerContext();
  service_worker_context->GetAllOriginsInfo(
      base::BindOnce(&IsolatedPrerenderServiceWorkersObserver::OnHasUsageInfo,
                     weak_factory_.GetWeakPtr()));

  observer_.Add(service_worker_context);
}

IsolatedPrerenderServiceWorkersObserver::
    ~IsolatedPrerenderServiceWorkersObserver() = default;

base::Optional<bool>
IsolatedPrerenderServiceWorkersObserver::IsServiceWorkerRegisteredForOrigin(
    const url::Origin& query_origin) const {
  if (!has_usage_info_) {
    return base::nullopt;
  }

  for (const url::Origin& origin : registered_origins_) {
    if (origin == query_origin) {
      return true;
    }
  }
  return false;
}

void IsolatedPrerenderServiceWorkersObserver::OnRegistrationCompleted(
    const GURL& scope) {
  registered_origins_.insert(url::Origin::Create(scope));
}

void IsolatedPrerenderServiceWorkersObserver::OnDestruct(
    content::ServiceWorkerContext* context) {
  observer_.Remove(context);
}

void IsolatedPrerenderServiceWorkersObserver::CallOnHasUsageInfoForTesting(
    const std::vector<content::StorageUsageInfo>& usage_info) {
  OnHasUsageInfo(usage_info);
}

void IsolatedPrerenderServiceWorkersObserver::OnHasUsageInfo(
    const std::vector<content::StorageUsageInfo>& usage_info) {
  has_usage_info_ = true;
  for (const content::StorageUsageInfo& info : usage_info) {
    registered_origins_.insert(info.origin);
  }
}
