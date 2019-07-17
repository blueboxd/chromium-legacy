// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT ApplicationCacheHostForFrame : public ApplicationCacheHost {
 public:
  ApplicationCacheHostForFrame(
      DocumentLoader* document_loader,
      mojom::blink::DocumentInterfaceBroker* interface_broker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // ApplicationCacheHost:
  bool Update() override;
  bool SwapCache() override;
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;

  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override;

  void WillStartLoadingMainResource(DocumentLoader* loader,
                                    const KURL& url,
                                    const String& method) override;
  void SelectCacheWithoutManifest() override;
  void SelectCacheWithManifest(const KURL& manifest_url) override;
  void DidReceiveResponseForMainResource(const ResourceResponse&) override;

  void Trace(blink::Visitor*) override;

 private:
  enum IsNewMasterEntry { MAYBE_NEW_ENTRY, NEW_ENTRY, OLD_ENTRY };

  Member<LocalFrame> local_frame_;
  bool is_get_method_ = false;
  bool was_select_cache_called_ = false;
  IsNewMasterEntry is_new_master_entry_ = MAYBE_NEW_ENTRY;
  bool is_scheme_supported_ = false;
  ResourceResponse document_response_;
  KURL document_url_;
  KURL original_main_resource_url_;  // Used to detect redirection.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
