// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKER_MODULE_SCRIPT_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKER_MODULE_SCRIPT_FETCHER_H_

#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"

namespace blink {

class WorkerGlobalScope;

// WorkerModuleScriptFetcher is an implementation of ModuleScriptFetcher
// interface for WebWorkers. This implements the custom "perform the fetch" hook
// defined in the HTML spec:
// https://html.spec.whatwg.org/C/#fetching-scripts-perform-fetch
// https://html.spec.whatwg.org/C/#worker-processing-model
class CORE_EXPORT WorkerModuleScriptFetcher final
    : public GarbageCollected<WorkerModuleScriptFetcher>,
      public ModuleScriptFetcher {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerModuleScriptFetcher);

 public:
  WorkerModuleScriptFetcher(WorkerGlobalScope*,
                            util::PassKey<ModuleScriptLoader>);

  // Implements ModuleScriptFetcher.
  void Fetch(FetchParameters&,
             ResourceFetcher*,
             ModuleGraphLevel,
             ModuleScriptFetcher::Client*) override;

  void Trace(Visitor*) override;

 private:
  // Implements ResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "WorkerModuleScriptFetcher"; }

  const Member<WorkerGlobalScope> global_scope_;

  Member<ResourceFetcher> fetch_client_settings_object_fetcher_;
  Member<Client> client_;
  ModuleGraphLevel level_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKER_MODULE_SCRIPT_FETCHER_H_
