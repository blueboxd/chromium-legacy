// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_shared_worker.h"

namespace blink {

ApplicationCacheHostForSharedWorker::ApplicationCacheHostForSharedWorker(
    ApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ApplicationCacheHostHelper(nullptr /* LocalFrame* */,
                                 client,
                                 appcache_host_id,
                                 std::move(task_runner)) {}

ApplicationCacheHostForSharedWorker::~ApplicationCacheHostForSharedWorker() =
    default;

void ApplicationCacheHostForSharedWorker::WillStartMainResourceRequest(
    const KURL& url,
    const String& method,
    const ApplicationCacheHostHelper* spawning_host) {}

void ApplicationCacheHostForSharedWorker::DidReceiveResponseForMainResource(
    const ResourceResponse&) {}

void ApplicationCacheHostForSharedWorker::SelectCacheWithoutManifest() {}

bool ApplicationCacheHostForSharedWorker::SelectCacheWithManifest(
    const KURL& manifestURL) {
  return true;
}

void ApplicationCacheHostForSharedWorker::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {}

void ApplicationCacheHostForSharedWorker::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {}

}  // namespace blink
