// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_helper.h"

namespace blink {
class ApplicationCacheHostForFrame final : public ApplicationCacheHostHelper {
 public:
  ApplicationCacheHostForFrame(
      LocalFrame* local_frame,
      ApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // mojom::blink::AppCacheHostFrontend:
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;

  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override;

  void Trace(blink::Visitor*) override;

 private:
  Member<LocalFrame> local_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
