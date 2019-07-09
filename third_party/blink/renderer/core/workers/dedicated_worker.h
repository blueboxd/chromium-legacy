// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_dedicated_worker.h"
#include "third_party/blink/public/platform/web_dedicated_worker_host_factory_client.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/abstract_worker.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_options.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class DedicatedWorkerMessagingProxy;
class ExceptionState;
class ExecutionContext;
class PostMessageOptions;
class ScriptState;
class WorkerClassicScriptLoader;
class WorkerClients;

// Implementation of the Worker interface defined in the WebWorker HTML spec:
// https://html.spec.whatwg.org/C/#worker
//
// Confusingly, the Worker interface is for dedicated workers, so this class is
// called DedicatedWorker. This lives on the thread that created the worker (the
// thread that called `new Worker()`), i.e., the main thread if a document
// created the worker or a worker thread in the case of nested workers.
//
// We have been rearchitecting the worker startup sequence, and now there are
// 3 paths as follows. The path is chosen based on runtime flags:
//
//  A) Legacy on-the-main-thread worker script loading (default)
//  - DedicatedWorker::Start()
//    - (Async script loading on the main thread)
//      - DedicatedWorker::OnFinished()
//        - DedicatedWorker::ContinueStart()
//
//  B) Off-the-main-thread worker script loading
//     (kOffMainThreadDedicatedWorkerScriptFetch)
//  - DedicatedWorker::Start()
//    - DedicatedWorker::ContinueStart()
//      - (Async script loading on the worker thread)
//
//  C) Off-the-main-thread worker script loading w/ PlzDedicatedWorker
//     (kOffMainThreadDedicatedWorkerScriptFetch + kPlzDedicatedWorker +
//      kNetworkService)
//  - DedicatedWorker::Start()
//    - (Start script loading in the browser)
//      - DedicatedWorker::OnScriptLoadStarted()
//        - DedicatedWorker::ContinueStart()
//          - (Async script loading on the worker thread)
class CORE_EXPORT DedicatedWorker final
    : public AbstractWorker,
      public ActiveScriptWrappable<DedicatedWorker>,
      public WebDedicatedWorker {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DedicatedWorker);
  // Pre-finalization is needed to notify the parent object destruction of the
  // GC-managed messaging proxy and to initiate worker termination.
  USING_PRE_FINALIZER(DedicatedWorker, Dispose);

 public:
  static DedicatedWorker* Create(ExecutionContext*,
                                 const String& url,
                                 const WorkerOptions*,
                                 ExceptionState&);

  DedicatedWorker(ExecutionContext*,
                  const KURL& script_request_url,
                  const WorkerOptions*);
  ~DedicatedWorker() override;

  void Dispose();

  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   Vector<ScriptValue>& transfer,
                   ExceptionState&);
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   const PostMessageOptions*,
                   ExceptionState&);
  void terminate();
  BeginFrameProviderParams CreateBeginFrameProviderParams();

  // Implements ContextLifecycleObserver (via AbstractWorker).
  void ContextDestroyed(ExecutionContext*) override;

  // Implements ScriptWrappable
  // (via AbstractWorker -> EventTargetWithInlineData -> EventTarget).
  bool HasPendingActivity() const final;

  // Implements WebDedicatedWorker.
  // Called only when PlzDedicatedWorker is enabled.
  void OnWorkerHostCreated(
      mojo::ScopedMessagePipeHandle interface_provider) override;
  void OnScriptLoadStarted() override;
  void OnScriptLoadStartFailed() override;

  void DispatchErrorEventForScriptFetchFailure();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message, kMessage)

  void ContextLifecycleStateChanged(mojom::FrameLifecycleState state) override;
  void Trace(blink::Visitor*) override;

 private:
  // Starts the worker.
  void Start();
  void ContinueStart(
      const KURL& script_url,
      OffMainThreadWorkerScriptFetchOption,
      network::mojom::ReferrerPolicy,
      base::Optional<mojom::IPAddressSpace> response_address_space,
      const String& source_code);
  std::unique_ptr<GlobalScopeCreationParams> CreateGlobalScopeCreationParams(
      const KURL& script_url,
      OffMainThreadWorkerScriptFetchOption,
      network::mojom::ReferrerPolicy,
      base::Optional<mojom::IPAddressSpace> response_address_space);
  scoped_refptr<WebWorkerFetchContext> CreateWebWorkerFetchContext();
  WorkerClients* CreateWorkerClients();

  // Callbacks for |classic_script_loader_|.
  void OnResponse();
  void OnFinished();

  // Implements EventTarget (via AbstractWorker -> EventTargetWithInlineData).
  const AtomicString& InterfaceName() const final;

  const KURL script_request_url_;
  Member<const WorkerOptions> options_;
  Member<const FetchClientSettingsObjectSnapshot>
      outside_fetch_client_settings_object_;
  const Member<DedicatedWorkerMessagingProxy> context_proxy_;

  Member<WorkerClassicScriptLoader> classic_script_loader_;

  // Used only when PlzDedicatedWorker is enabled.
  std::unique_ptr<WebDedicatedWorkerHostFactoryClient> factory_client_;

  // Used for tracking cross-debugger calls.
  v8_inspector::V8StackTraceId v8_stack_trace_id_;

  service_manager::mojom::blink::InterfaceProviderPtrInfo interface_provider_;

  // Whether the worker is frozen due to a call from this context.
  bool requested_frozen_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_H_
