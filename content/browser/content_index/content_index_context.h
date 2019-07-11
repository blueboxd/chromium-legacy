// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "content/browser/content_index/content_index_database.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// Owned by the Storage Partition. Components that want to query or modify the
// Content Index database should hold a reference to this.
class CONTENT_EXPORT ContentIndexContext
    : public base::RefCountedThreadSafe<ContentIndexContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  ContentIndexContext(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  void InitializeOnIOThread();
  void Shutdown();

  ContentIndexDatabase& database();

 private:
  friend class base::DeleteHelper<ContentIndexContext>;
  friend class base::RefCountedThreadSafe<ContentIndexContext,
                                          BrowserThread::DeleteOnIOThread>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  ~ContentIndexContext();

  ContentIndexDatabase content_index_database_;

  // Whether initialization DB tasks should run on start-up.
  bool should_initialize_ = false;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_H_
