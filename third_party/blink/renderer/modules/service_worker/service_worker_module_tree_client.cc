// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_module_tree_client.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

ServiceWorkerModuleTreeClient::ServiceWorkerModuleTreeClient(
    Modulator* modulator)
    : modulator_(modulator) {}

// This client is used for both new and installed scripts. In the new scripts
// case, this is a partial implementation of the custom "perform the fetch" hook
// in the spec: https://w3c.github.io/ServiceWorker/#update-algorithm For
// installed scripts, there is no corresponding specification text because there
// is no fetching process there. The service worker simply uses its associated
// script resource.
void ServiceWorkerModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  auto* execution_context =
      ExecutionContext::From(modulator_->GetScriptState());
  auto* worker_global_scope = To<WorkerGlobalScope>(execution_context);
  blink::WorkerReportingProxy& worker_reporting_proxy =
      worker_global_scope->ReportingProxy();

  if (!module_script) {
    // (In the update case) Step 9: "If the algorithm asynchronously completes
    // with null, then: Invoke Reject Job Promise with job and TypeError."
    // DidFailToFetchModuleScript() signals that startup failed, which causes
    // ServiceWorkerRegisterJob to reject the job promise.
    worker_reporting_proxy.DidFailToFetchModuleScript();
    worker_global_scope->close();
    return;
  }
  worker_reporting_proxy.DidFetchScript();

  // (In the update case) Step 9: "Else, continue the rest of these steps after
  // the algorithm's asynchronous completion, with script being the asynchronous
  // completion value."
  worker_global_scope->WorkerScriptFetchFinished(
      *module_script, base::nullopt /* v8_inspector::V8StackTraceId */);
}

void ServiceWorkerModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink
