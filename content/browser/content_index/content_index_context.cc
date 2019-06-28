// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

ContentIndexContext::ContentIndexContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : content_index_database_(browser_context,
                              std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content_index_database_.InitializeProviderWithEntries();
}

void ContentIndexContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexContext::ShutdownOnIO, this));
}

void ContentIndexContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content_index_database_.Shutdown();
}

ContentIndexDatabase& ContentIndexContext::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return content_index_database_;
}

ContentIndexContext::~ContentIndexContext() = default;

}  // namespace content
