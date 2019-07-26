// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/content_index_provider.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;

// Handles interacting with the Service Worker Database for Content Index
// entries. This is owned by the ContentIndexContext.
class CONTENT_EXPORT ContentIndexDatabase {
 public:
  ContentIndexDatabase(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~ContentIndexDatabase();

  void AddEntry(int64_t service_worker_registration_id,
                const url::Origin& origin,
                blink::mojom::ContentDescriptionPtr description,
                const SkBitmap& icon,
                const GURL& launch_url,
                blink::mojom::ContentIndexService::AddCallback callback);

  void DeleteEntry(int64_t service_worker_registration_id,
                   const url::Origin& origin,
                   const std::string& entry_id,
                   blink::mojom::ContentIndexService::DeleteCallback callback);

  void GetDescriptions(
      int64_t service_worker_registration_id,
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback);

  // Gets the icon for |description_id| and invokes |icon_callback| on the UI
  // thread.
  void GetIcon(int64_t service_worker_registration_id,
               const std::string& description_id,
               base::OnceCallback<void(SkBitmap)> icon_callback);

  // Returns all registered entries.
  void GetAllEntries(ContentIndexContext::GetAllEntriesCallback callback);

  // Returns the specified entry.
  void GetEntry(int64_t service_worker_registration_id,
                const std::string& description_id,
                ContentIndexContext::GetEntryCallback callback);

  // Block/Unblock DB operations for |origin|.
  void BlockOrigin(const url::Origin& origin);
  void UnblockOrigin(const url::Origin& origin);

  // Called when the storage partition is shutting down.
  void Shutdown();

  base::WeakPtr<ContentIndexDatabase> GetWeakPtrForIO() {
    return weak_ptr_factory_io_.GetWeakPtr();
  }

 private:
  void DidSerializeIcon(int64_t service_worker_registration_id,
                        const url::Origin& origin,
                        blink::mojom::ContentDescriptionPtr description,
                        const GURL& launch_url,
                        blink::mojom::ContentIndexService::AddCallback callback,
                        std::string serialized_icon);
  void DidAddEntry(blink::mojom::ContentIndexService::AddCallback callback,
                   ContentIndexEntry entry,
                   blink::ServiceWorkerStatusCode status);
  void DidDeleteEntry(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string& entry_id,
      blink::mojom::ContentIndexService::DeleteCallback callback,
      blink::ServiceWorkerStatusCode status);
  void DidGetDescriptions(
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);
  void DidGetSerializedIcon(base::OnceCallback<void(SkBitmap)> icon_callback,
                            const std::vector<std::string>& data,
                            blink::ServiceWorkerStatusCode status);
  void DidGetEntries(
      ContentIndexContext::GetAllEntriesCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);
  void DidGetEntry(int64_t service_worker_registration_id,
                   ContentIndexContext::GetEntryCallback callback,
                   const std::vector<std::string>& data,
                   blink::ServiceWorkerStatusCode status);

  // Callbacks on the UI thread to notify |provider_| of updates.
  void NotifyProviderContentAdded(std::vector<ContentIndexEntry> entries);
  void NotifyProviderContentDeleted(int64_t service_worker_registration_id,
                                    const url::Origin& origin,
                                    const std::string& entry_id);

  // Lives on the UI thread.
  ContentIndexProvider* provider_;

  // A map from origins to how many times it's been blocked.
  base::flat_map<url::Origin, int> blocked_origins_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<ContentIndexDatabase> weak_ptr_factory_io_;
  base::WeakPtrFactory<ContentIndexDatabase> weak_ptr_factory_ui_;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
