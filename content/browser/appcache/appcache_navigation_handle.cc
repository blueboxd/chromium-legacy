// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_thread.h"

namespace content {

AppCacheNavigationHandle::AppCacheNavigationHandle(
    ChromeAppCacheService* appcache_service,
    int process_id)
    : appcache_host_id_(base::UnguessableToken::Create()),
      core_(std::make_unique<AppCacheNavigationHandleCore>(appcache_service,
                                                           appcache_host_id_,
                                                           process_id)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_->Initialize();
}

AppCacheNavigationHandle::~AppCacheNavigationHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void AppCacheNavigationHandle::SetProcessId(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_->SetProcessId(process_id);
}

}  // namespace content
