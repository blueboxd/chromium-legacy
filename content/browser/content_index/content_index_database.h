// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT ContentIndexDatabase {
 public:
  ContentIndexDatabase(
      const url::Origin& origin,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~ContentIndexDatabase();

  void AddEntry(int64_t service_worker_registration_id,
                blink::mojom::ContentDescriptionPtr description,
                const SkBitmap& icon,
                blink::mojom::ContentIndexService::AddCallback callback);

  void DeleteEntry(int64_t service_worker_registration_id,
                   const std::string& entry_id,
                   blink::mojom::ContentIndexService::DeleteCallback callback);

  void GetDescriptions(
      int64_t service_worker_registration_id,
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback);

 private:
  void DidAddEntry(blink::mojom::ContentIndexService::AddCallback callback,
                   blink::ServiceWorkerStatusCode status);
  void DidDeleteEntry(
      blink::mojom::ContentIndexService::DeleteCallback callback,
      blink::ServiceWorkerStatusCode status);
  void DidGetDescriptions(
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);

  url::Origin origin_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<ContentIndexDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
