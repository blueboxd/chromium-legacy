// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker_task_queue_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::BrowserContext;

namespace extensions {

namespace {

// A preference key storing the information about an extension that was
// activated and has a registered worker based background page.
const char kPrefServiceWorkerRegistrationInfo[] =
    "service_worker_registration_info";

// The extension version of the registered service worker.
const char kServiceWorkerVersion[] = "version";

ServiceWorkerTaskQueue::TestObserver* g_test_observer = nullptr;

void DidStartWorkerFail() {
  DCHECK(false) << "DidStartWorkerFail";
  // TODO(lazyboy): Handle failure case.
}

}  // namespace

ServiceWorkerTaskQueue::ServiceWorkerTaskQueue(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ServiceWorkerTaskQueue::~ServiceWorkerTaskQueue() {}

ServiceWorkerTaskQueue::TestObserver::TestObserver() {}

ServiceWorkerTaskQueue::TestObserver::~TestObserver() {}

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueue::Get(BrowserContext* context) {
  return ServiceWorkerTaskQueueFactory::GetForBrowserContext(context);
}

// static
void ServiceWorkerTaskQueue::DidStartWorkerForScopeOnIO(
    const LazyContextId& context_id,
    base::WeakPtr<ServiceWorkerTaskQueue> task_queue,
    int64_t version_id,
    int process_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerForScope,
                     task_queue, context_id, version_id, process_id,
                     thread_id));
}

// static
void ServiceWorkerTaskQueue::StartServiceWorkerOnIOToRunTasks(
    base::WeakPtr<ServiceWorkerTaskQueue> task_queue_weak,
    const LazyContextId& context_id,
    content::ServiceWorkerContext* service_worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  service_worker_context->StartWorkerForScope(
      context_id.service_worker_scope(),
      base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerForScopeOnIO,
                     context_id, std::move(task_queue_weak)),
      base::BindOnce(&DidStartWorkerFail));
}

void ServiceWorkerTaskQueue::DidStartWorkerForScope(
    const LazyContextId& context_id,
    int64_t version_id,
    int process_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  loaded_workers_.insert(context_id);
  RunPendingTasksIfWorkerReady(context_id, version_id, process_id, thread_id);
}

void ServiceWorkerTaskQueue::DidInitializeServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessManager::Get(browser_context_)
      ->RegisterServiceWorker({extension_id, render_process_id,
                               service_worker_version_id, thread_id});
}

void ServiceWorkerTaskQueue::DidStartServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LazyContextId context_id(browser_context_, extension_id,
                           service_worker_scope);
  started_workers_.insert(context_id);
  RunPendingTasksIfWorkerReady(context_id, service_worker_version_id,
                               render_process_id, thread_id);
}

void ServiceWorkerTaskQueue::DidStopServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessManager::Get(browser_context_)
      ->UnregisterServiceWorker({extension_id, render_process_id,
                                 service_worker_version_id, thread_id});
  LazyContextId context_id(browser_context_, extension_id,
                           service_worker_scope);
  // TODO(lazyboy): Run orphaned tasks with nullptr ContextInfo.
  pending_tasks_.erase(context_id);

  loaded_workers_.erase(context_id);
  started_workers_.erase(context_id);
}

// static
void ServiceWorkerTaskQueue::SetObserverForTest(TestObserver* observer) {
  g_test_observer = observer;
}

bool ServiceWorkerTaskQueue::ShouldEnqueueTask(BrowserContext* context,
                                               const Extension* extension) {
  // We call StartWorker every time we want to dispatch an event to an extension
  // Service worker.
  // TODO(lazyboy): Is that a problem?
  return true;
}

void ServiceWorkerTaskQueue::AddPendingTask(const LazyContextId& context_id,
                                            PendingTask task) {
  DCHECK(context_id.is_for_service_worker());

  // TODO(lazyboy): Do we need to handle incognito context?

  auto& tasks = pending_tasks_[context_id];
  bool needs_start_worker = tasks.empty();
  tasks.push_back(std::move(task));

  if (pending_registrations_.count(context_id.extension_id()) > 0) {
    // If the worker hasn't finished registration, wait for it to complete.
    // DidRegisterServiceWorker will Start worker to run |task| later.
    return;
  }

  // Start worker if there isn't any request to start worker with |context_id|
  // is in progress.
  if (needs_start_worker)
    RunTasksAfterStartWorker(context_id);
}

void ServiceWorkerTaskQueue::ActivateExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const ExtensionId extension_id = extension->id();
  // Note: version.IsValid() = false implies we didn't have any prefs stored.
  base::Version version = RetrieveRegisteredServiceWorkerVersion(extension_id);
  const bool service_worker_already_registered =
      version.IsValid() && version == extension->version();
  if (g_test_observer) {
    g_test_observer->OnActivateExtension(extension_id,
                                         !service_worker_already_registered);
  }

  if (service_worker_already_registered) {
    // TODO(https://crbug.com/901101): We should kick off an async check to see
    // if the registration is *actually* there and re-register if necessary.
    return;
  }

  pending_registrations_.insert(extension_id);
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  blink::mojom::ServiceWorkerRegistrationOptions option;
  option.scope = extension->url();
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->RegisterServiceWorker(
          script_url, option,
          base::BindOnce(&ServiceWorkerTaskQueue::DidRegisterServiceWorker,
                         weak_factory_.GetWeakPtr(), extension_id));
}

void ServiceWorkerTaskQueue::DeactivateExtension(const Extension* extension) {
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  const ExtensionId extension_id = extension->id();
  RemoveRegisteredServiceWorkerInfo(extension_id);

  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->UnregisterServiceWorker(
          extension->url(),
          base::BindOnce(&ServiceWorkerTaskQueue::DidUnregisterServiceWorker,
                         weak_factory_.GetWeakPtr(), extension_id));
}

void ServiceWorkerTaskQueue::RunTasksAfterStartWorker(
    const LazyContextId& context_id) {
  DCHECK(context_id.is_for_service_worker());

  if (context_id.browser_context() != browser_context_)
    return;

  content::StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(
          context_id.browser_context(), context_id.service_worker_scope());
  content::ServiceWorkerContext* service_worker_context =
      partition->GetServiceWorkerContext();

  content::ServiceWorkerContext::RunTask(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      FROM_HERE, service_worker_context,
      base::BindOnce(&ServiceWorkerTaskQueue::StartServiceWorkerOnIOToRunTasks,
                     weak_factory_.GetWeakPtr(), context_id,
                     service_worker_context));
}

void ServiceWorkerTaskQueue::DidRegisterServiceWorker(
    const ExtensionId& extension_id,
    bool success) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  LazyContextId context_id(browser_context_, extension_id,
                           // NOTE: Background page service worker's scope.
                           extension->url());

  if (!success) {
    // TODO(lazyboy): Handle failure case thoroughly.
    pending_tasks_.erase(context_id);
    return;
  }

  SetRegisteredServiceWorkerInfo(extension->id(), extension->version());
  pending_registrations_.erase(extension_id);

  if (!pending_tasks_[context_id].empty()) {
    // TODO(lazyboy): If worker for |context_id| is already running, consider
    // not calling StartWorker. This isn't straightforward as service worker's
    // internal state is mostly on IO thread.
    RunTasksAfterStartWorker(context_id);
  }
}

void ServiceWorkerTaskQueue::DidUnregisterServiceWorker(
    const ExtensionId& extension_id,
    bool success) {
  // TODO(lazyboy): Handle success = false case.
  if (!success)
    LOG(ERROR) << "Failed to unregister service worker!";
}

base::Version ServiceWorkerTaskQueue::RetrieveRegisteredServiceWorkerVersion(
    const ExtensionId& extension_id) {
  const base::DictionaryValue* info = nullptr;
  if (!ExtensionPrefs::Get(browser_context_)
           ->ReadPrefAsDictionary(extension_id,
                                  kPrefServiceWorkerRegistrationInfo, &info)) {
    return base::Version();
  }
  std::string version_string;
  info->GetString(kServiceWorkerVersion, &version_string);
  return base::Version(version_string);
}

void ServiceWorkerTaskQueue::SetRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id,
    const base::Version& version) {
  DCHECK(version.IsValid());
  auto info = std::make_unique<base::DictionaryValue>();
  info->SetString(kServiceWorkerVersion, version.GetString());
  ExtensionPrefs::Get(browser_context_)
      ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                            std::move(info));
}

void ServiceWorkerTaskQueue::RemoveRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id) {
  ExtensionPrefs::Get(browser_context_)
      ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                            nullptr);
}

void ServiceWorkerTaskQueue::RunPendingTasksIfWorkerReady(
    const LazyContextId& context_id,
    int64_t version_id,
    int process_id,
    int thread_id) {
  if (!base::Contains(loaded_workers_, context_id) ||
      !base::Contains(started_workers_, context_id)) {
    // The worker hasn't finished starting (DidStartWorkerForScope) or hasn't
    // finished loading (DidStartServiceWorkerContext).
    // Wait for the next event, and run the tasks then.
    return;
  }

  // Running |pending_tasks_[context_id]| marks the completion of
  // DidStartWorkerForScope, clean up |loaded_workers_| so that new tasks can be
  // queued up.
  loaded_workers_.erase(context_id);

  auto iter = pending_tasks_.find(context_id);
  DCHECK(iter != pending_tasks_.end()) << "Worker ready, but no tasks to run!";
  std::vector<PendingTask> tasks = std::move(iter->second);
  pending_tasks_.erase(iter);
  for (auto& task : tasks) {
    auto context_info = std::make_unique<LazyContextTaskQueue::ContextInfo>(
        context_id.extension_id(),
        content::RenderProcessHost::FromID(process_id), version_id, thread_id,
        context_id.service_worker_scope());
    std::move(task).Run(std::move(context_info));
  }
}

}  // namespace extensions
