// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_HELPER_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ApplicationCacheHostClient;

// TODO(https://crbug.com/950159): Combine ApplicationCacheHostHelper and
// ApplicationCacheHost and make ApplicationCacheHostForFrame and
// ApplicationCacheHostForSharedWorker inherited from ApplicationCacheHost.
class CORE_EXPORT ApplicationCacheHostHelper
    : public GarbageCollectedFinalized<ApplicationCacheHostHelper>,
      public mojom::blink::AppCacheFrontend {
 public:
  static ApplicationCacheHostHelper* Create(
      LocalFrame* local_frame,
      ApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id);

  // This is only for testing in order to create it without any arguments.
  ApplicationCacheHostHelper();

  ApplicationCacheHostHelper(
      // |local_frame| is used for accessing to DocumentInterfaceBroker. As it's
      // not used for workers, |local_frame| is null for workers.
      LocalFrame* local_frame,
      ApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHostHelper() override;

  const base::UnguessableToken& host_id() const { return host_id_; }

  // mojom::blink::AppCacheFrontend
  void CacheSelected(mojom::blink::AppCacheInfoPtr info) override;
  void EventRaised(mojom::blink::AppCacheEventID event_id) override;
  void ProgressEventRaised(const KURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(mojom::blink::AppCacheErrorDetailsPtr details) override;
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override {}
  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override {}

  virtual void WillStartMainResourceRequest(
      const KURL& url,
      const String& method,
      const ApplicationCacheHostHelper* spawning_host);
  virtual void SelectCacheWithoutManifest();
  virtual bool SelectCacheWithManifest(const KURL& manifestURL);
  virtual void DidReceiveResponseForMainResource(const ResourceResponse&);

  mojom::blink::AppCacheStatus GetStatus();
  bool StartUpdate();
  bool SwapCache();
  void GetResourceList(Vector<mojom::blink::AppCacheResourceInfo>* resources);
  // Structures and methods to support inspecting Application Caches.
  struct CacheInfo {
    KURL manifest_url;  // Empty if there is no associated cache.
    double creation_time;
    double update_time;
    // Sums up the sizes of all the responses in this cache.
    int64_t response_sizes;
    // Sums up the padding sizes for all opaque responses in the cache.
    int64_t padding_sizes;
    CacheInfo()
        : creation_time(0),
          update_time(0),
          response_sizes(0),
          padding_sizes(0) {}
  };
  void GetAssociatedCacheInfo(CacheInfo* info);
  const base::UnguessableToken& GetHostID() const;
  void SelectCacheForSharedWorker(int64_t app_cache_id,
                                  base::OnceClosure completion_callback);
  void DetachFromDocumentLoader();
  virtual void Trace(blink::Visitor*) {}

 private:
  enum IsNewMasterEntry { MAYBE_NEW_ENTRY, NEW_ENTRY, OLD_ENTRY };

  mojo::Binding<mojom::blink::AppCacheFrontend> binding_;
  ApplicationCacheHostClient* client_;
  mojom::blink::AppCacheHostPtr backend_host_;
  base::UnguessableToken host_id_;
  mojom::blink::AppCacheStatus status_;
  ResourceResponse document_response_;
  KURL document_url_;
  bool is_scheme_supported_;
  bool is_get_method_;
  IsNewMasterEntry is_new_master_entry_;
  mojom::blink::AppCacheInfo cache_info_;
  KURL original_main_resource_url_;  // Used to detect redirection.
  bool was_select_cache_called_;
  // Invoked when CacheSelected() is called.
  base::OnceClosure select_cache_for_shared_worker_completion_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_HELPER_H_
