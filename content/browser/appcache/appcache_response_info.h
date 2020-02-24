// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_INFO_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_INFO_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "url/gurl.h"

namespace net {
class HttpResponseInfo;
}

namespace content {

class AppCacheStorage;

// Response info for a particular response id. Instances are tracked in
// the working set.
class CONTENT_EXPORT AppCacheResponseInfo
    : public base::RefCounted<AppCacheResponseInfo> {
 public:
  AppCacheResponseInfo(base::WeakPtr<AppCacheStorage> storage,
                       const GURL& manifest_url,
                       int64_t response_id,
                       std::unique_ptr<net::HttpResponseInfo> http_info,
                       int64_t response_data_size);

  const GURL& manifest_url() const { return manifest_url_; }
  int64_t response_id() const { return response_id_; }
  const net::HttpResponseInfo& http_response_info() const {
    return *http_response_info_;
  }
  int64_t response_data_size() const { return response_data_size_; }

 private:
  friend class base::RefCounted<AppCacheResponseInfo>;
  ~AppCacheResponseInfo();

  const GURL manifest_url_;
  const int64_t response_id_;
  const std::unique_ptr<net::HttpResponseInfo> http_response_info_;
  const int64_t response_data_size_;
  base::WeakPtr<AppCacheStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_INFO_H_
