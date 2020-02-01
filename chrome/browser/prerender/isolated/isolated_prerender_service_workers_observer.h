// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_WORKERS_OBSERVER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_WORKERS_OBSERVER_H_

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

// A helper class that queries and observes the ServiceWorkerContext in order to
// provide a synchronous lookup of registered service workers to the rest of
// this feature.
// Note:
// * Service worker de-registration is intentionally ignored. We don't want to
// trigger the Isolated Prerender feature when there has been a service worker
// for an origin at any time during the browsing session.
class IsolatedPrerenderServiceWorkersObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit IsolatedPrerenderServiceWorkersObserver(Profile* profile);
  ~IsolatedPrerenderServiceWorkersObserver() override;

  // Returns true if there is a service worker registered for the given
  // |query_origin| in the default storage partition, or false if there isn't
  // one. Returns nullopt when |this| is still waiting for ServiceWorkerContext
  // to provide a response.
  base::Optional<bool> IsServiceWorkerRegisteredForOrigin(
      const url::Origin& query_origin) const;

  // Publicly exposes |OnHasUsageInfo| for testing only.
  void CallOnHasUsageInfoForTesting(
      const std::vector<content::StorageUsageInfo>& usage_info);

 private:
  // content::ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  // content::GetUsageInfoCallback
  void OnHasUsageInfo(const std::vector<content::StorageUsageInfo>& usage_info);

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      observer_{this};

  // Set to true when |OnHasUsageInfo| is called.
  bool has_usage_info_ = false;

  // The origin of all registered service workers.
  std::set<url::Origin> registered_origins_;

  base::WeakPtrFactory<IsolatedPrerenderServiceWorkersObserver> weak_factory_{
      this};

  IsolatedPrerenderServiceWorkersObserver(
      const IsolatedPrerenderServiceWorkersObserver&) = delete;
  IsolatedPrerenderServiceWorkersObserver& operator=(
      const IsolatedPrerenderServiceWorkersObserver&) = delete;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_WORKERS_OBSERVER_H_
