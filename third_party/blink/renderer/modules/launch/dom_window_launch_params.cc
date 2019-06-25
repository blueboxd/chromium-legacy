// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/dom_window_launch_params.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const char DOMWindowLaunchParams::kSupplementName[] = "DOMWindowLaunchParams";

DOMWindowLaunchParams::DOMWindowLaunchParams(ExecutionContext* context) {}
DOMWindowLaunchParams::~DOMWindowLaunchParams() = default;

ScriptPromise DOMWindowLaunchParams::getLaunchParams(ScriptState* script_state,
                                                     LocalDOMWindow& window) {
  if (!base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI) ||
      !base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  return FromState(script_state, &window)->GetLaunchParams(script_state);
}

ScriptPromise DOMWindowLaunchParams::GetLaunchParams(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise resolver_result = resolver->Promise();

  // Immediately resolve the promise if the launch params are already known.
  // TODO(crbug.com/829689): Getting the launch params from the browser will be
  // asynchronous, so it will be possible that the |launch_params_| have not
  // been initialized. Once a launch service has been created, a list of
  // unresolved promises should be stored, so they can be resolved once the
  // service has responded to a request for launch params.
  resolver->Resolve(launch_params_);

  return resolver_result;
}

void DOMWindowLaunchParams::Trace(blink::Visitor* visitor) {
  visitor->Trace(launch_params_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowLaunchParams* DOMWindowLaunchParams::FromState(
    ScriptState* script_state,
    LocalDOMWindow* window) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DOMWindowLaunchParams* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowLaunchParams>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowLaunchParams>(context);
    ProvideTo(*window, supplement);

    // TODO(crbug.com/829689): When a service is created for getting launch
    // params from the browser process, use that instead of hard coding the
    // launch params.
    supplement->launch_params_ = MakeGarbageCollected<LaunchParams>(
        HeapVector<Member<NativeFileSystemHandle>>());
  }
  return supplement;
}

}  // namespace blink
