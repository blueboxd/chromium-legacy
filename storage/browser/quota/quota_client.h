// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "url/origin.h"

namespace storage {

// Interface between each storage API and the quota manager.
//
// Each storage API must register an implementation of this interface with
// the quota manager, by calling QuotaManager::RegisterClient().
//
// All the methods will be called on the IO thread in the browser.
//
// When AppCache is deleted, this can become a std::unique_ptr instead
// of refcounted, and owned by the QuotaManager.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaClient
    : public base::RefCountedThreadSafe<QuotaClient> {
 public:
  // The callback aliases precisely follow mojo conventions, because this
  // abstract class will become a mojo interface soon. See crbug.com/1016065.
  using GetOriginUsageCallback = base::OnceCallback<void(int64_t usage)>;
  using GetOriginsForTypeCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;
  using GetOriginsForHostCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;
  using DeleteOriginDataCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode status)>;
  using PerformStorageCleanupCallback = base::OnceClosure;

  // Called when the QuotaManager is destroyed.
  virtual void OnQuotaManagerDestroyed() = 0;

  // Called by the QuotaManager.
  // Gets the amount of data stored in the storage specified by
  // |origin| and |type|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginUsage(const url::Origin& origin,
                              blink::mojom::StorageType type,
                              GetOriginUsageCallback callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that has data in the |type| storage.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForType(blink::mojom::StorageType type,
                                 GetOriginsForTypeCallback callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that match the |host|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForHost(blink::mojom::StorageType type,
                                 const std::string& host,
                                 GetOriginsForHostCallback callback) = 0;

  // Called by the QuotaManager.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void DeleteOriginData(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                DeleteOriginDataCallback callback) = 0;

  // Called by the QuotaManager.
  // Gives the QuotaClient an opportunity to perform a cleanup step after major
  // deletions.
  virtual void PerformStorageCleanup(
      blink::mojom::StorageType type,
      PerformStorageCleanupCallback callback) = 0;

 protected:
  friend class RefCountedThreadSafe<QuotaClient>;

  virtual ~QuotaClient() = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
