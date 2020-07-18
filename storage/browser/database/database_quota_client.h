// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class DatabaseTracker;

// Integrates WebSQL databases with the quota management system.
//
// This interface is used on the IO thread by the quota manager.
class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseQuotaClient
    : public QuotaClient {
 public:
  explicit DatabaseQuotaClient(scoped_refptr<DatabaseTracker> tracker);

  // QuotaClient method overrides
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetOriginUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsForTypeCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeleteOriginDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  ~DatabaseQuotaClient() override;

  scoped_refptr<DatabaseTracker> db_tracker_;  // only used on its sequence

  DISALLOW_COPY_AND_ASSIGN(DatabaseQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
