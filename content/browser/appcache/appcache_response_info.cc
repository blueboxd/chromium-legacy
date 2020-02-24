// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_response_info.h"

#include <stddef.h>

#include <utility>

#include "content/browser/appcache/appcache_storage.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_info.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-forward.h"

namespace content {

AppCacheResponseInfo::AppCacheResponseInfo(
    base::WeakPtr<AppCacheStorage> storage,
    const GURL& manifest_url,
    int64_t response_id,
    std::unique_ptr<net::HttpResponseInfo> http_info,
    int64_t response_data_size)
    : manifest_url_(manifest_url),
      response_id_(response_id),
      http_response_info_(std::move(http_info)),
      response_data_size_(response_data_size),
      storage_(std::move(storage)) {
  DCHECK(http_response_info_);
  DCHECK(response_id != blink::mojom::kAppCacheNoResponseId);
  storage_->working_set()->AddResponseInfo(this);
}

AppCacheResponseInfo::~AppCacheResponseInfo() {
  if (storage_)
    storage_->working_set()->RemoveResponseInfo(this);
}

}  // namespace content
