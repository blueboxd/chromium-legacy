// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// static
GPUFence* GPUFence::Create(GPUDevice* device, DawnFence fence) {
  return MakeGarbageCollected<GPUFence>(device, fence);
}

GPUFence::GPUFence(GPUDevice* device, DawnFence fence)
    : DawnObject<DawnFence>(device, fence) {}

GPUFence::~GPUFence() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().fenceRelease(GetHandle());
}

uint64_t GPUFence::getCompletedValue() const {
  return GetProcs().fenceGetCompletedValue(GetHandle());
}

void GPUFence::OnCompletionCallback(ScriptPromiseResolver* resolver,
                                    DawnFenceCompletionStatus status) {
  switch (status) {
    case DAWN_FENCE_COMPLETION_STATUS_SUCCESS:
      resolver->Resolve();
      break;
    case DAWN_FENCE_COMPLETION_STATUS_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    case DAWN_FENCE_COMPLETION_STATUS_UNKNOWN:
    case DAWN_FENCE_COMPLETION_STATUS_DEVICE_LOST:
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GPUFence::onCompletion(ScriptState* script_state,
                                     uint64_t value) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindDawnCallback(&GPUFence::OnCompletionCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  GetProcs().fenceOnCompletion(GetHandle(), value, callback->UnboundCallback(),
                               callback->AsUserdata());

  return promise;
}

}  // namespace blink
