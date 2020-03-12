// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_NAMESPACE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_NAMESPACE_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

enum AppCacheNamespaceType {
  APPCACHE_FALLBACK_NAMESPACE,
  APPCACHE_INTERCEPT_NAMESPACE,
  APPCACHE_NETWORK_NAMESPACE,
};

struct CONTENT_EXPORT AppCacheNamespace {
  AppCacheNamespace();  // Type is APPCACHE_FALLBACK_NAMESPACE by default.
  AppCacheNamespace(AppCacheNamespaceType type,
                    const GURL& url,
                    const GURL& target);
  ~AppCacheNamespace();

  bool IsMatch(const GURL& url) const;

  AppCacheNamespaceType type;
  GURL namespace_url;
  GURL target_url;
  base::Time token_expires;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_NAMESPACE_H_
