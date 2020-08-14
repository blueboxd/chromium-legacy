// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/renderer_resource_coordinator_impl.h"

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

RendererResourceCoordinatorImpl::~RendererResourceCoordinatorImpl() = default;

// static
void RendererResourceCoordinatorImpl::MaybeInitialize() {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled())
    return;

  blink::Platform* platform = Platform::Current();
  DCHECK(IsMainThread());
  DCHECK(platform);

  mojo::PendingRemote<
      performance_manager::mojom::blink::ProcessCoordinationUnit>
      remote;
  platform->GetBrowserInterfaceBroker()->GetInterface(
      remote.InitWithNewPipeAndPassReceiver());
  RendererResourceCoordinator::Set(
      new RendererResourceCoordinatorImpl(std::move(remote)));
}

void RendererResourceCoordinatorImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  if (!service_)
    return;
  service_->SetMainThreadTaskLoadIsLow(main_thread_task_load_is_low);
}

void RendererResourceCoordinatorImpl::OnScriptStateCreated(
    ScriptState* script_state,
    ExecutionContext* execution_context) {
  // TODO(chrisha): Extract tokens and forward this to the browser!
}

void RendererResourceCoordinatorImpl::OnScriptStateDetached(
    ScriptState* script_state) {
  // TODO(chrisha): Extract tokens and forward this to the browser!
}

void RendererResourceCoordinatorImpl::OnScriptStateDestroyed(
    ScriptState* script_state) {
  // TODO(chrisha): Extract tokens and forward this to the browser!
}

RendererResourceCoordinatorImpl::RendererResourceCoordinatorImpl(
    mojo::PendingRemote<
        performance_manager::mojom::blink::ProcessCoordinationUnit> remote) {
  service_.Bind(std::move(remote));
}

}  // namespace blink
