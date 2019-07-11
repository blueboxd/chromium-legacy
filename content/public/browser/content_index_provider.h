// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

class SkBitmap;

namespace content {

struct CONTENT_EXPORT ContentIndexEntry {
  ContentIndexEntry(int64_t service_worker_registration_id,
                    blink::mojom::ContentDescriptionPtr description,
                    base::Time registration_time);
  ContentIndexEntry(ContentIndexEntry&& other);
  ~ContentIndexEntry();

  // Part of the key for an entry since different service workers can use the
  // same ID.
  int64_t service_worker_registration_id;

  // All the developer provided information.
  blink::mojom::ContentDescriptionPtr description;

  // The time the registration was created.
  base::Time registration_time;
};

// Interface for content providers to receive content-related updates.
class CONTENT_EXPORT ContentIndexProvider {
 public:
  // Interface for the client that updates the provider with entries.
  class CONTENT_EXPORT Client {
   public:
    virtual ~Client();

    // The client will need to provide an icon for the entry when requested.
    // Must be called on the UI thread. |icon_callback| must be invoked on the
    // UI thread.
    virtual void GetIcon(int64_t service_worker_registration_id,
                         const std::string& description_id,
                         base::OnceCallback<void(SkBitmap)> icon_callback) = 0;
  };

  ContentIndexProvider();
  virtual ~ContentIndexProvider();

  // Called when a new entry is registered. |client| is passed for when the
  // provider will require additional information relating to the entry.
  // Must be called on the UI thread.
  virtual void OnContentAdded(
      ContentIndexEntry entry,
      base::WeakPtr<ContentIndexProvider::Client> client) = 0;

  // Called when an entry is unregistered. Must be called on the UI thread.
  virtual void OnContentDeleted(int64_t service_worker_registration_id,
                                const std::string& description_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexProvider);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_
