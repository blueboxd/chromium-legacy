// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"

class SkBitmap;

namespace content {

// Owned by the Storage Partition. This is used by the ContentIndexProvider to
// query auxiliary data for its entries from the right source.
class CONTENT_EXPORT ContentIndexContext {
 public:
  ContentIndexContext() = default;
  virtual ~ContentIndexContext() = default;

  // The client will need to provide an icon for the entry when requested.
  // Must be called on the UI thread. |icon_callback| must be invoked on the
  // UI thread.
  virtual void GetIcon(int64_t service_worker_registration_id,
                       const std::string& description_id,
                       base::OnceCallback<void(SkBitmap)> icon_callback) = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
