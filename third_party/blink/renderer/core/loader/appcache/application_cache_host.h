/*
 * Copyright (c) 2009, Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class ApplicationCache;
class DocumentLoader;
class ResourceRequest;

class CORE_EXPORT ApplicationCacheHost
    : public GarbageCollectedFinalized<ApplicationCacheHost>,
      public mojom::blink::AppCacheFrontend {
 public:
  static ApplicationCacheHost* Create(DocumentLoader* document_loader);
  // |interface_broker| can be null for workers and |task_runner| is null for
  // kAppCacheForNone.
  explicit ApplicationCacheHost(
      DocumentLoader* document_loader,
      mojom::blink::DocumentInterfaceBroker* interface_broker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHost() override;
  void DetachFromDocumentLoader();

  struct CacheInfo {
    STACK_ALLOCATED();

   public:
    CacheInfo(const KURL& manifest,
              double creation_time,
              double update_time,
              int64_t response_sizes,
              int64_t padding_sizes)
        : manifest_(manifest),
          creation_time_(creation_time),
          update_time_(update_time),
          response_sizes_(response_sizes),
          padding_sizes_(padding_sizes) {}
    CacheInfo() = default;
    KURL manifest_;
    double creation_time_ = 0;
    double update_time_ = 0;
    int64_t response_sizes_ = 0;
    int64_t padding_sizes_ = 0;
  };

  // Annotate request for ApplicationCache.
  void WillStartLoading(ResourceRequest&);

  mojom::blink::AppCacheStatus GetStatus() const;
  virtual bool Update() { return false; }
  virtual bool SwapCache() { return false; }
  void Abort();

  void SetApplicationCache(ApplicationCache*);

  void
  StopDeferringEvents();  // Also raises the events that have been queued up.

  void FillResourceList(Vector<mojom::blink::AppCacheResourceInfo>*);
  CacheInfo ApplicationCacheInfo();
  const base::UnguessableToken& GetHostID() const;
  void SelectCacheForSharedWorker(int64_t app_cache_id,
                                  base::OnceClosure completion_callback);

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

  // TODO(nhiroki): Move these virtual functions into
  // ApplicationCacheHostForFrame after making DocumentLoader own only
  // ApplicationCacheHostForFrame (not own ApplicationCacheHostForSharedWorker).
  virtual void WillStartLoadingMainResource(DocumentLoader* loader,
                                            const KURL& url,
                                            const String& method);
  virtual void SelectCacheWithoutManifest() {}
  virtual void SelectCacheWithManifest(const KURL& manifest_url) {}
  virtual void DidReceiveResponseForMainResource(const ResourceResponse&) {}
  virtual void Trace(blink::Visitor*);

 protected:
  DocumentLoader* GetDocumentLoader() const { return document_loader_; }

  mojo::Remote<mojom::blink::AppCacheHost> backend_host_;
  mojom::blink::AppCacheStatus status_ =
      mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;

 private:
  void NotifyApplicationCache(mojom::AppCacheEventID,
                              int progress_total,
                              int progress_done,
                              mojom::AppCacheErrorReason,
                              const String& error_url,
                              int error_status,
                              const String& error_message);

  void GetAssociatedCacheInfo(CacheInfo* info);
  bool IsApplicationCacheEnabled();
  bool BindBackend();
  void DispatchDOMEvent(mojom::AppCacheEventID,
                        int progress_total,
                        int progress_done,
                        mojom::AppCacheErrorReason,
                        const String& error_url,
                        int error_status,
                        const String& error_message);

  struct DeferredEvent {
    mojom::AppCacheEventID event_id;
    int progress_total;
    int progress_done;
    mojom::AppCacheErrorReason error_reason;
    String error_url;
    int error_status;
    String error_message;
    DeferredEvent(mojom::AppCacheEventID id,
                  int progress_total,
                  int progress_done,
                  mojom::AppCacheErrorReason error_reason,
                  const String& error_url,
                  int error_status,
                  const String& error_message)
        : event_id(id),
          progress_total(progress_total),
          progress_done(progress_done),
          error_reason(error_reason),
          error_url(error_url),
          error_status(error_status),
          error_message(error_message) {}
  };

  WeakMember<ApplicationCache> dom_application_cache_ = nullptr;

  // TODO(https://crbug.com/982996): Move this to ApplicationCacheHostForFrame.
  Member<DocumentLoader> document_loader_;

  bool defers_events_ =
      true;  // Events are deferred until after document onload.
  Vector<DeferredEvent> deferred_events_;
  mojo::Receiver<mojom::blink::AppCacheFrontend> receiver_{this};
  base::UnguessableToken host_id_;
  mojom::blink::AppCacheInfo cache_info_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  mojom::blink::DocumentInterfaceBroker* interface_broker_;
  // Invoked when CacheSelected() is called.
  base::OnceClosure select_cache_for_shared_worker_completion_callback_;

  FRIEND_TEST_ALL_PREFIXES(DocumentTest, SandboxDisablesAppCache);

  DISALLOW_COPY_AND_ASSIGN(ApplicationCacheHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
