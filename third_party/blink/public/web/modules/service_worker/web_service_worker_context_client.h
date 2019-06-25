/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-shared.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "v8/include/v8.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {

class WebServiceWorkerContextProxy;
class WebServiceWorkerNetworkProvider;
class WebString;

// Used to pass the mojom struct blink.mojom.FetchEventPreloadHandle across the
// boundary between //content and Blink.
struct WebFetchEventPreloadHandle {
  // For network::mojom::URLLoaderPtrInfo.
  mojo::ScopedMessagePipeHandle url_loader;
  // For network::mojom::URLLoaderClientRequest.
  mojo::ScopedMessagePipeHandle url_loader_client_request;
};

// WebServiceWorkerContextClient is a "client" of a service worker execution
// context. This interface is implemented by the embedder and allows the
// embedder to communicate with the service worker execution context.  It is
// created on the main thread and then passed on to the worker thread by a newly
// created ServiceWorkerGlobalScope.
//
// Unless otherwise noted, all methods of this class are called on the worker
// thread.
class WebServiceWorkerContextClient {
 public:
  using RequestTerminationCallback = base::OnceCallback<void(bool)>;

  virtual ~WebServiceWorkerContextClient() = default;

  // ServiceWorker has prepared everything for script loading and is now ready
  // for DevTools inspection. Called on the main thread.
  virtual void WorkerReadyForInspectionOnMainThread() {}

  // Starting the worker failed. This could happen when loading the worker
  // script failed, or the worker was asked to terminate before startup
  // completed. Called on the main thread.
  virtual void WorkerContextFailedToStartOnMainThread() {}

  // The worker started but it could not execute because loading the classic
  // script failed on the worker thread. This is called only for installed
  // scripts fetch or off-the-main-thread classic worker script fetch.
  virtual void FailedToLoadClassicScript() {}

  // The worker started but it could not execute because fetching module script
  // failed.
  virtual void FailedToFetchModuleScript() {}

  // The worker script was successfully loaded by ResourceLoader. Called on the
  // main thread.
  //
  // This is called before WorkerContextStarted(). Script evaluation does not
  // start until WillEvaluateScript().
  virtual void WorkerScriptLoadedOnMainThread() {}

  // The worker script was successfully loaded on the worker thread.
  // When off-the-main-thread script fetch is on, this is called for both
  // new-script and installed-script cases. If off-the-main-thread script fetch
  // is off, this is called for only the installed-script case.
  //
  // This is called after WorkerContextStarted(). Script evaluation does not
  // start until WillEvaluateScript().
  virtual void WorkerScriptLoadedOnWorkerThread() {}

  // Called when a WorkerGlobalScope was created for the worker thread. This
  // also gives a proxy to the embedder to talk to the newly created
  // WorkerGlobalScope. The proxy is owned by WorkerGlobalScope and should not
  // be destroyed by the caller. No proxy methods should be called after
  // willDestroyWorkerContext() is called.
  //
  // |worker_task_runner| is a task runner that runs tasks on the worker thread
  // and safely discards tasks when the thread stops. See
  // blink::WorkerThread::GetTaskRunner().
  //
  // For new workers (on-main-thread script fetch), this is called after
  // WorkerScriptLoadedOnWorkerThread().
  //
  // For installed workers, this is called before
  // WorkerScriptLoadedOnMainThread().
  //
  // Script evaluation does not start until WillEvaluateScript().
  virtual void WorkerContextStarted(
      WebServiceWorkerContextProxy*,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {}

  // Called immediately before V8 script evaluation starts for the main script.
  // This means all setup is finally complete: the script has been loaded, the
  // worker thread has started, the script has been passed to the worker thread,
  // and CSP and ReferrerPolicy information has been set on the worker thread.
  virtual void WillEvaluateScript() {}

  // Called when initial script evaluation finished for the main script.
  // |success| is true if the evaluation completed with no uncaught exception.
  virtual void DidEvaluateScript(bool success) {}

  // Called when the worker context is going to be initialized. This is the
  // initial method call after creating the worker scheduler.
  virtual void WillInitializeWorkerContext() {}

  // Called when the worker context is initialized. This is probably called
  // after WorkerContextStarted(). (WorkerThread::InitializeOnWorkerThread()
  // calls WorkerContextStarted() via
  // WorkerReportingProxy::DidCreateWorkerGlobalScope(),
  // and then initializes the worker context if "needed" and calls
  // DidInitializeWorkerContext(), but it's not clear when the context would
  // already be initialized.)
  //
  // |context_proxy| is valid until WillDestroyWorkerContext() is called.
  //
  // This function is used to support service workers in Chrome extensions.
  //
  // TODO(nhiroki): Can you clarify this code and comment?
  virtual void DidInitializeWorkerContext(
      WebServiceWorkerContextProxy* context_proxy,
      v8::Local<v8::Context> v8_context) {}

  // WorkerGlobalScope is about to be destroyed. The client should clear
  // the WebServiceWorkerGlobalScopeProxy when this is called.
  virtual void WillDestroyWorkerContext(v8::Local<v8::Context> context) {}

  // WorkerGlobalScope was destroyed and the worker is ready to be terminated.
  virtual void WorkerContextDestroyed() {}

  // Called when some API to be recorded in UseCounter is called on the worker
  // global scope.
  virtual void CountFeature(mojom::WebFeature feature) {}

  // Called when the WorkerGlobalScope had an error or an exception.
  virtual void ReportException(const WebString& error_message,
                               int line_number,
                               int column_number,
                               const WebString& source_url) {}

  // Called when a console message was written.
  virtual void ReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                                    blink::mojom::ConsoleMessageLevel level,
                                    const WebString& message,
                                    int line_number,
                                    const WebString& source_url) {}

  // Called when the navigation preload (FetchEvent#preloadResponse) is needed.
  virtual void SetupNavigationPreload(
      int fetch_event_id,
      const WebURL& url,
      std::unique_ptr<WebFetchEventPreloadHandle> preload_handle) {}

  // Called when we need to request to terminate this worker due to idle
  // timeout.
  virtual void RequestTermination(RequestTerminationCallback) {}

  // On-main-thread start up:
  // Creates a network provider for the main script fetch.
  // This is called on the main thread.
  virtual std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProviderOnMainThread() = 0;

  // On-main-thread start up:
  // Creates a WebWorkerFetchContext for subresource fetches on a service
  // worker. This is called on the main thread.
  virtual scoped_refptr<blink::WebWorkerFetchContext>
  CreateWorkerFetchContextOnMainThreadLegacy(WebServiceWorkerNetworkProvider*) {
    return nullptr;
  }

  // Off-main-thread start up:
  // Creates a WebWorkerFetchContext for subresource fetches on a service
  // worker. This is called on the main thread.
  virtual scoped_refptr<blink::WebWorkerFetchContext>
  CreateWorkerFetchContextOnMainThread() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_CLIENT_H_
