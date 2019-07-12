// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}

namespace extensions {
class Extension;
class LazyContextId;

// TODO(lazyboy): Clean up queue when extension is unloaded/uninstalled.
// TODO(lazyboy): Describe the flow of a task in this queue: i.e. when
// a worker receives DidRegisterServiceWorker, DidStartWorkerForScope and
// DidStartServiceWorkerContext events and how queued tasks react to these
// events.
class ServiceWorkerTaskQueue : public KeyedService,
                               public LazyContextTaskQueue {
 public:
  explicit ServiceWorkerTaskQueue(content::BrowserContext* browser_context);
  ~ServiceWorkerTaskQueue() override;

  // Convenience method to return the ServiceWorkerTaskQueue for a given
  // |context|.
  static ServiceWorkerTaskQueue* Get(content::BrowserContext* context);

  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension) override;
  void AddPendingTask(const LazyContextId& context_id,
                      PendingTask task) override;

  // Performs Service Worker related tasks upon |extension| activation,
  // e.g. registering |extension|'s worker, executing any pending tasks.
  void ActivateExtension(const Extension* extension);
  // Performs Service Worker related tasks upon |extension| deactivation,
  // e.g. unregistering |extension|'s worker.
  void DeactivateExtension(const Extension* extension);

  // Called once an extension Service Worker context was initialized but not
  // necessarily started executing its JavaScript.
  void DidInitializeServiceWorkerContext(int render_process_id,
                                         const ExtensionId& extension_id,
                                         int64_t service_worker_version_id,
                                         int thread_id);
  // Called once an extension Service Worker started running.
  // This can be thought as "loadstop", i.e. the global JS script of the worker
  // has completed executing.
  void DidStartServiceWorkerContext(int render_process_id,
                                    const ExtensionId& extension_id,
                                    const GURL& service_worker_scope,
                                    int64_t service_worker_version_id,
                                    int thread_id);
  // Called once an extension Service Worker was destroyed.
  void DidStopServiceWorkerContext(int render_process_id,
                                   const ExtensionId& extension_id,
                                   const GURL& service_worker_scope,
                                   int64_t service_worker_version_id,
                                   int thread_id);

  class TestObserver {
   public:
    TestObserver();
    virtual ~TestObserver();

    // Called when an extension with id |extension_id| is going to be activated.
    // |will_register_service_worker| is true if a Service Worker will be
    // registered.
    virtual void OnActivateExtension(const ExtensionId& extension_id,
                                     bool will_register_service_worker) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(TestObserver);
  };

  static void SetObserverForTest(TestObserver* observer);

 private:
  static void DidStartWorkerForScopeOnIO(
      const LazyContextId& context_id,
      base::WeakPtr<ServiceWorkerTaskQueue> task_queue,
      int64_t version_id,
      int process_id,
      int thread_id);
  static void StartServiceWorkerOnIOToRunTasks(
      base::WeakPtr<ServiceWorkerTaskQueue> task_queue_weak,
      const LazyContextId& context_id,
      content::ServiceWorkerContext* service_worker_context);

  void RunTasksAfterStartWorker(const LazyContextId& context_id);

  void DidRegisterServiceWorker(const ExtensionId& extension_id, bool success);
  void DidUnregisterServiceWorker(const ExtensionId& extension_id,
                                  bool success);

  void DidStartWorkerForScope(const LazyContextId& context_id,
                              int64_t version_id,
                              int process_id,
                              int thread_id);

  // The following three methods retrieve, store, and remove information
  // about Service Worker registration of SW based background pages:
  //
  // Retrieves the last version of the extension, returns invalid version if
  // there isn't any such extension.
  base::Version RetrieveRegisteredServiceWorkerVersion(
      const ExtensionId& extension_id);
  // Records that the extension with |extension_id| and |version| successfully
  // registered a Service Worker.
  void SetRegisteredServiceWorkerInfo(const ExtensionId& extension_id,
                                      const base::Version& version);
  // Clears any record of registered Service Worker for the given extension with
  // |extension_id|.
  void RemoveRegisteredServiceWorkerInfo(const ExtensionId& extension_id);

  // If the worker with |context_id| has seen worker start
  // (DidStartWorkerForScope) and load (DidStartServiceWorkerContext) then runs
  // all pending tasks for that worker.
  void RunPendingTasksIfWorkerReady(const LazyContextId& context_id,
                                    int64_t version_id,
                                    int process_id,
                                    int thread_id);

  // Set of extension ids that hasn't completed Service Worker registration.
  std::set<ExtensionId> pending_registrations_;

  // Set of workers that has seen DidStartWorkerForScope.
  std::set<LazyContextId> started_workers_;

  // Set of workers that has seen DidStartServiceWorkerContext.
  std::set<LazyContextId> loaded_workers_;

  // Pending tasks for a |LazyContextId|.
  // These tasks will be run once the corresponding worker becomes ready.
  std::map<LazyContextId, std::vector<PendingTask>> pending_tasks_;

  content::BrowserContext* const browser_context_ = nullptr;

  base::WeakPtrFactory<ServiceWorkerTaskQueue> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTaskQueue);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_
