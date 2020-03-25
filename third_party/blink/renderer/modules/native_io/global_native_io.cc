// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/global_native_io.h"

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/native_io/native_io_manager.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalNativeIOImpl final : public GarbageCollected<GlobalNativeIOImpl<T>>,
                                 public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalNativeIOImpl);

 public:
  static const char kSupplementName[];

  static GlobalNativeIOImpl& From(T& supplementable) {
    GlobalNativeIOImpl* supplement =
        Supplement<T>::template From<GlobalNativeIOImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalNativeIOImpl>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  explicit GlobalNativeIOImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  NativeIOManager* GetNativeIOManager(T& scope) {
    if (!native_io_manager_) {
      ExecutionContext* execution_context = scope.GetExecutionContext();
      if (&execution_context->GetBrowserInterfaceBroker() ==
          &GetEmptyBrowserInterfaceBroker()) {
        return nullptr;
      }

      mojo::Remote<mojom::blink::NativeIOHost> backend;
      execution_context->GetBrowserInterfaceBroker().GetInterface(
          backend.BindNewPipeAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
      native_io_manager_ = MakeGarbageCollected<NativeIOManager>(
          execution_context, std::move(backend));
    }
    return native_io_manager_;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(native_io_manager_);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<NativeIOManager> native_io_manager_;
};

// static
template <typename T>
const char GlobalNativeIOImpl<T>::kSupplementName[] = "GlobalNativeIOImpl";

}  // namespace

// static
NativeIOManager* GlobalNativeIO::nativeIO(LocalDOMWindow& window) {
  return GlobalNativeIOImpl<LocalDOMWindow>::From(window).GetNativeIOManager(
      window);
}

// static
NativeIOManager* GlobalNativeIO::nativeIO(WorkerGlobalScope& worker) {
  return GlobalNativeIOImpl<WorkerGlobalScope>::From(worker).GetNativeIOManager(
      worker);
}

}  // namespace blink
