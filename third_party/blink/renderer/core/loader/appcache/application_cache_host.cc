/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/renderer/core/events/application_cache_error_event.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_shared_worker.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// We provide a custom implementation of this class that calls out to the
// embedding application instead of using WebCore's built in appcache system.
// This file replaces webcore/appcache/ApplicationCacheHost.cpp in our build.

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader* document_loader)
    : dom_application_cache_(nullptr),
      document_loader_(document_loader),
      defers_events_(true) {
  DCHECK(document_loader_);
}

ApplicationCacheHost::~ApplicationCacheHost() {
  // Verify that detachFromDocumentLoader() has been performed already.
  DCHECK(!helper_);
}

void ApplicationCacheHost::WillStartLoading(ResourceRequest& request) {
  if (!IsApplicationCacheEnabled() || !helper_)
    return;
  const base::UnguessableToken& host_id = helper_->GetHostID();
  if (!host_id.is_empty())
    request.SetAppCacheHostID(host_id);
}

void ApplicationCacheHost::WillStartLoadingMainResource(DocumentLoader* loader,
                                                        const KURL& url,
                                                        const String& method) {
  if (!IsApplicationCacheEnabled())
    return;
  // We defer creating the outer host object to avoid spurious
  // creation/destruction around creating empty documents. At this point, we're
  // initiating a main resource load for the document, so its for real.

  DCHECK(document_loader_->GetFrame());
  LocalFrame& frame = *document_loader_->GetFrame();

  helper_ = ApplicationCacheHostHelper::Create(&frame, this,
                                               loader->AppcacheHostId());
  if (!helper_)
    return;

  const ApplicationCacheHostHelper* spawning_host_helper = nullptr;
  Frame* spawning_frame = frame.Tree().Parent();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame.Loader().Opener();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = &frame;
  if (DocumentLoader* spawning_doc_loader =
          To<LocalFrame>(spawning_frame)->Loader().GetDocumentLoader()) {
    spawning_host_helper =
        spawning_doc_loader->GetApplicationCacheHost()
            ? spawning_doc_loader->GetApplicationCacheHost()->helper_.Get()
            : nullptr;
  }

  helper_->WillStartMainResourceRequest(url, method, spawning_host_helper);

  // NOTE: The semantics of this method, and others in this interface, are
  // subtly different than the method names would suggest. For example, in this
  // method never returns an appcached response in the SubstituteData out
  // argument, instead we return the appcached response thru the usual resource
  // loading pipeline.
}

void ApplicationCacheHost::SelectCacheWithoutManifest() {
  if (helper_)
    helper_->SelectCacheWithoutManifest();
}

void ApplicationCacheHost::SelectCacheWithManifest(const KURL& manifest_url) {
  DCHECK(document_loader_);

  LocalFrame* frame = document_loader_->GetFrame();
  Document* document = frame->GetDocument();
  if (document->IsSandboxed(WebSandboxFlags::kOrigin)) {
    // Prevent sandboxes from establishing application caches.
    SelectCacheWithoutManifest();
    return;
  }
  if (document->IsSecureContext()) {
    UseCounter::Count(document,
                      WebFeature::kApplicationCacheManifestSelectSecureOrigin);
  } else {
    Deprecation::CountDeprecation(
        document, WebFeature::kApplicationCacheManifestSelectInsecureOrigin);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document, WebFeature::kApplicationCacheManifestSelectInsecureOrigin);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::
                       kApplicationCacheManifestSelectInsecureHost);
  }
  if (helper_ && !helper_->SelectCacheWithManifest(manifest_url)) {
    // It's a foreign entry, restart the current navigation from the top of the
    // navigation algorithm. The navigation will not result in the same resource
    // being loaded, because "foreign" entries are never picked during
    // navigation. see ApplicationCacheGroup::selectCache()
    FrameLoadRequest request(document, ResourceRequest(document->Url()));
    request.SetClientRedirectReason(ClientNavigationReason::kReload);
    frame->Navigate(request, WebFrameLoadType::kReplaceCurrentItem);
  }
}

void ApplicationCacheHost::DidReceiveResponseForMainResource(
    const ResourceResponse& response) {
  if (helper_) {
    helper_->DidReceiveResponseForMainResource(response);
  }
}

void ApplicationCacheHost::SetApplicationCache(
    ApplicationCache* dom_application_cache) {
  DCHECK(!dom_application_cache_ || !dom_application_cache);
  dom_application_cache_ = dom_application_cache;
}

void ApplicationCacheHost::DetachFromDocumentLoader() {
  // Detach from the owning DocumentLoader and let go of
  // ApplicationCacheHostHelper.
  SetApplicationCache(nullptr);
  if (helper_) {
    helper_->DetachFromDocumentLoader();
    helper_.Clear();
  }
  document_loader_ = nullptr;
}

void ApplicationCacheHost::NotifyApplicationCache(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  if (id != mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  }

  if (defers_events_) {
    // Event dispatching is deferred until document.onload has fired.
    deferred_events_.push_back(DeferredEvent(id, progress_total, progress_done,
                                             error_reason, error_url,
                                             error_status, error_message));
    return;
  }
  DispatchDOMEvent(id, progress_total, progress_done, error_reason, error_url,
                   error_status, error_message);
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::ApplicationCacheInfo() {
  if (!helper_)
    return CacheInfo(NullURL(), 0, 0, 0, 0);

  ApplicationCacheHostHelper::CacheInfo cache_info;
  helper_->GetAssociatedCacheInfo(&cache_info);
  return CacheInfo(cache_info.manifest_url, cache_info.creation_time,
                   cache_info.update_time, cache_info.response_sizes,
                   cache_info.padding_sizes);
}

const base::UnguessableToken& ApplicationCacheHost::GetHostID() const {
  if (!helper_)
    return base::UnguessableToken::Null();
  return helper_->GetHostID();
}

void ApplicationCacheHost::SelectCacheForSharedWorker(
    int64_t app_cache_id,
    base::OnceClosure completion_callback) {
  helper_->SelectCacheForSharedWorker(app_cache_id,
                                      std::move(completion_callback));
}

void ApplicationCacheHost::FillResourceList(
    Vector<mojom::blink::AppCacheResourceInfo>* resources) {
  DCHECK(resources);
  if (!helper_)
    return;

  helper_->GetResourceList(resources);
}

void ApplicationCacheHost::StopDeferringEvents() {
  for (unsigned i = 0; i < deferred_events_.size(); ++i) {
    const DeferredEvent& deferred = deferred_events_[i];
    DispatchDOMEvent(deferred.event_id, deferred.progress_total,
                     deferred.progress_done, deferred.error_reason,
                     deferred.error_url, deferred.error_status,
                     deferred.error_message);
  }
  deferred_events_.clear();
  defers_events_ = false;
}

void ApplicationCacheHost::DispatchDOMEvent(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  // Don't dispatch an event if the window is detached.
  if (!dom_application_cache_ || !dom_application_cache_->DomWindow())
    return;

  const AtomicString& event_type = ApplicationCache::ToEventType(id);
  if (event_type.IsEmpty())
    return;
  Event* event = nullptr;
  if (id == mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    event =
        ProgressEvent::Create(event_type, true, progress_done, progress_total);
  } else if (id == mojom::AppCacheEventID::APPCACHE_ERROR_EVENT) {
    event = MakeGarbageCollected<ApplicationCacheErrorEvent>(
        error_reason, error_url, error_status, error_message);
  } else {
    event = Event::Create(event_type);
  }
  dom_application_cache_->DispatchEvent(*event);
}

mojom::AppCacheStatus ApplicationCacheHost::GetStatus() const {
  return helper_ ? helper_->GetStatus()
                 : mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
}

bool ApplicationCacheHost::Update() {
  return helper_ ? helper_->StartUpdate() : false;
}

bool ApplicationCacheHost::SwapCache() {
  bool success = helper_ ? helper_->SwapCache() : false;
  if (success) {
    probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  }
  return success;
}

void ApplicationCacheHost::Abort() {
}

bool ApplicationCacheHost::IsApplicationCacheEnabled() {
  DCHECK(document_loader_->GetFrame());
  return document_loader_->GetFrame()->GetSettings() &&
         document_loader_->GetFrame()
             ->GetSettings()
             ->GetOfflineWebApplicationCacheEnabled();
}

void ApplicationCacheHost::DidChangeCacheAssociation() {
  // FIXME: Prod the inspector to update its notion of what cache the page is
  // using.
}

void ApplicationCacheHost::NotifyEventListener(
    mojom::AppCacheEventID event_id) {
  NotifyApplicationCache(event_id, 0, 0,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::NotifyProgressEventListener(const KURL&,
                                                       int progress_total,
                                                       int progress_done) {
  NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT,
                         progress_total, progress_done,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::NotifyErrorEventListener(
    mojom::AppCacheErrorReason reason,
    const KURL& url,
    int status,
    const String& message) {
  NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_ERROR_EVENT, 0, 0,
                         reason, url.GetString(), status, message);
}

void ApplicationCacheHost::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_application_cache_);
  visitor->Trace(document_loader_);
  visitor->Trace(helper_);
}

}  // namespace blink
