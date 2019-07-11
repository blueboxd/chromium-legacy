// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

ContentIndexContext::ContentIndexContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : content_index_database_(browser_context,
                              std::move(service_worker_context)),
      should_initialize_(browser_context->GetContentIndexProvider()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!should_initialize_)
    return;

  content_index_database_.InitializeProviderWithEntries();
}

void ContentIndexContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content_index_database_.Shutdown();
}

ContentIndexDatabase& ContentIndexContext::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return content_index_database_;
}

ContentIndexContext::~ContentIndexContext() = default;

}  // namespace content
