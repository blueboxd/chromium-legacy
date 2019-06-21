// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_shared_worker.h"

namespace blink {

ApplicationCacheHostForSharedWorker::ApplicationCacheHostForSharedWorker(
    DocumentLoader* document_loader,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ApplicationCacheHost(document_loader,
                           nullptr, /* interface_broker */
                           std::move(task_runner)) {}

ApplicationCacheHostForSharedWorker::~ApplicationCacheHostForSharedWorker() =
    default;

void ApplicationCacheHostForSharedWorker::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {}

void ApplicationCacheHostForSharedWorker::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {}

}  // namespace blink
