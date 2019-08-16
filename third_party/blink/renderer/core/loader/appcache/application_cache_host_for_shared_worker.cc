// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_shared_worker.h"

namespace blink {

ApplicationCacheHostForSharedWorker::ApplicationCacheHostForSharedWorker(
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ApplicationCacheHost(nullptr, /* interface_broker */
                           std::move(task_runner)) {
  SetHostID(appcache_host_id ? appcache_host_id
                             : base::UnguessableToken::Create());
  BindBackend();
}

ApplicationCacheHostForSharedWorker::~ApplicationCacheHostForSharedWorker() =
    default;

}  // namespace blink
