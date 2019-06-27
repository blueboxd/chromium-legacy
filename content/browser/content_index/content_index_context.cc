// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context.h"

namespace content {

ContentIndexContext::ContentIndexContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : content_index_database_(std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

ContentIndexDatabase& ContentIndexContext::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return content_index_database_;
}

ContentIndexContext::~ContentIndexContext() = default;

}  // namespace content
