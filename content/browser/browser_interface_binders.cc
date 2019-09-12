// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_interface_binders.h"

#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/screen_enumeration/screen_enumeration_impl.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/shared_worker_instance.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"

namespace content {
namespace internal {

// Documents/frames
void PopulateFrameBinders(RenderFrameHostImpl* host,
                          service_manager::BinderMap* map) {
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &RenderFrameHostImpl::CreateAppCacheBackend, base::Unretained(host)));

  map->Add<blink::mojom::AudioContextManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetAudioContextManager, base::Unretained(host)));

  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileSystemManager, base::Unretained(host)));

  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetIdleManager, base::Unretained(host)));

  map->Add<blink::mojom::PresentationService>(base::BindRepeating(
      &RenderFrameHostImpl::GetPresentationService, base::Unretained(host)));

  map->Add<blink::mojom::SpeechSynthesis>(base::BindRepeating(
      &RenderFrameHostImpl::GetSpeechSynthesis, base::Unretained(host)));

  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));

  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderFrameHostImpl::CreateLockManager, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    RenderFrameHostImpl* host,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map) {
  map->Add<blink::mojom::BackgroundFetchService>(
      base::BindRepeating(&BackgroundFetchServiceImpl::CreateForFrame));
  GetContentClient()->browser()->RegisterBrowserInterfaceBindersForFrame(map);
}

void PopulateBinderMap(RenderFrameHostImpl* host,
                       service_manager::BinderMap* map) {
  PopulateFrameBinders(host, map);
}

RenderFrameHost* GetContextForHost(RenderFrameHostImpl* host) {
  return host;
}

// Dedicated workers
const url::Origin& GetContextForHost(DedicatedWorkerHost* host) {
  return host->GetOrigin();
}

void PopulateDedicatedWorkerBinders(DedicatedWorkerHost* host,
                                    service_manager::BinderMap* map) {
  // base::Unretained(host) is safe because the map is owned by
  // |DedicatedWorkerHost::broker_|.
  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &DedicatedWorkerHost::BindFileSystemManager, base::Unretained(host)));
  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &DedicatedWorkerHost::CreateIdleManager, base::Unretained(host)));
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
}

void PopulateBinderMapWithContext(
    DedicatedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  map->Add<blink::mojom::LockManager>(
      base::BindRepeating(&RenderProcessHost::CreateLockManager,
                          base::Unretained(host->GetProcessHost())));
}

void PopulateBinderMap(DedicatedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateDedicatedWorkerBinders(host, map);
}

// Shared workers
url::Origin GetContextForHost(SharedWorkerHost* host) {
  return url::Origin::Create(host->instance().url());
}

void PopulateSharedWorkerBinders(SharedWorkerHost* host,
                                 service_manager::BinderMap* map) {
  // base::Unretained(host) is safe because the map is owned by
  // |SharedWorkerHost::broker_|.
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &SharedWorkerHost::CreateAppCacheBackend, base::Unretained(host)));
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
}

void PopulateBinderMapWithContext(
    SharedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  // TODO(https://crbug.com/873661): Pass origin to FileSystemManager.
  map->Add<blink::mojom::FileSystemManager>(
      base::BindRepeating(&RenderProcessHost::BindFileSystemManager,
                          base::Unretained(host->GetProcessHost())));
  map->Add<blink::mojom::LockManager>(
      base::BindRepeating(&RenderProcessHost::CreateLockManager,
                          base::Unretained(host->GetProcessHost())));
}

void PopulateBinderMap(SharedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateSharedWorkerBinders(host, map);
}

// Service workers
ServiceWorkerVersionInfo GetContextForHost(ServiceWorkerProviderHost* host) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  return host->running_hosted_version()->GetInfo();
}

void PopulateServiceWorkerBinders(ServiceWorkerProviderHost* host,
                                  service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));

  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &ServiceWorkerProviderHost::CreateLockManager, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    ServiceWorkerProviderHost* host,
    service_manager::BinderMapWithContext<const ServiceWorkerVersionInfo&>*
        map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Use a task runner if ServiceWorkerProviderHost lives on the IO
  // thread, as CreateForWorker() needs to be called on the UI thread.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker));
  } else {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
  }
}

void PopulateBinderMap(ServiceWorkerProviderHost* host,
                       service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  PopulateServiceWorkerBinders(host, map);
}

}  // namespace internal
}  // namespace content
