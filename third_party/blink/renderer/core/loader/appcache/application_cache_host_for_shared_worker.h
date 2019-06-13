// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_helper.h"

namespace blink {

class ApplicationCacheHostForSharedWorker final
    : public ApplicationCacheHostHelper {
 public:
  ApplicationCacheHostForSharedWorker(
      ApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHostForSharedWorker() override;

  // Main resource loading is different for workers. The main resource is
  // loaded by the worker using WorkerClassicScriptLoader.
  // These overrides are stubbed out.
  void WillStartMainResourceRequest(
      const KURL& url,
      const String& method,
      const ApplicationCacheHostHelper* spawning_host) override;
  void DidReceiveResponseForMainResource(const ResourceResponse&) override;

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  void SelectCacheWithoutManifest() override;
  bool SelectCacheWithManifest(const KURL& manifestURL) override;

  // mojom::blink::AppCacheFrontend:
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;
  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
