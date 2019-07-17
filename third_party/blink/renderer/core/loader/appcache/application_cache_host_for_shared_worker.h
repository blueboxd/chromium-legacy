// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

namespace blink {

class ApplicationCacheHostForSharedWorker final : public ApplicationCacheHost {
 public:
  ApplicationCacheHostForSharedWorker(
      DocumentLoader* document_loader,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHostForSharedWorker() override;

  // ApplicationCacheHost:
  bool Update() override;
  bool SwapCache() override;
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;
  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override;
  void SelectCacheWithoutManifest() override;
  void SelectCacheWithManifest(const KURL& manifest_url) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
