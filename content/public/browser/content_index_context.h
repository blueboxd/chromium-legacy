// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_index_provider.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

class SkBitmap;

namespace url {
class Origin;
}  // namespace url

namespace content {

// Owned by the Storage Partition. This is used by the ContentIndexProvider to
// query auxiliary data for its entries from the right source.
class CONTENT_EXPORT ContentIndexContext {
 public:
  using GetAllEntriesCallback =
      base::OnceCallback<void(blink::mojom::ContentIndexError,
                              std::vector<ContentIndexEntry>)>;
  using GetEntryCallback =
      base::OnceCallback<void(base::Optional<ContentIndexEntry>)>;

  ContentIndexContext() = default;
  virtual ~ContentIndexContext() = default;

  // The client will need to provide an icon for the entry when requested.
  // Must be called on the UI thread. |icon_callback| must be invoked on the
  // UI thread.
  virtual void GetIcon(int64_t service_worker_registration_id,
                       const std::string& description_id,
                       base::OnceCallback<void(SkBitmap)> icon_callback) = 0;

  // Must be called on the UI thread.
  virtual void GetAllEntries(GetAllEntriesCallback callback) = 0;

  // Must be called on the UI thread.
  virtual void GetEntry(int64_t service_worker_registration_id,
                        const std::string& description_id,
                        GetEntryCallback callback) = 0;

  // Called when a user deleted an item. Must be called on the UI thread.
  virtual void OnUserDeletedItem(int64_t service_worker_registration_id,
                                 const url::Origin& origin,
                                 const std::string& description_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
