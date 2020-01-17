// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_namespace.h"

#include <string>

#include "base/strings/string_util.h"

namespace content {

AppCacheNamespace::AppCacheNamespace() : type(APPCACHE_FALLBACK_NAMESPACE) {}

AppCacheNamespace::AppCacheNamespace(AppCacheNamespaceType type,
                                     const GURL& url,
                                     const GURL& target)
    : type(type), namespace_url(url), target_url(target) {}

AppCacheNamespace::~AppCacheNamespace() = default;

bool AppCacheNamespace::IsMatch(const GURL& url) const {
  return base::StartsWith(url.spec(), namespace_url.spec(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace content
