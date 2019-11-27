// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace {
const char kCannotCancelHitTestSource[] =
    "Hit test source could not be canceled! Ensure that it was not already "
    "canceled.";
}

namespace blink {

XRHitTestSource::XRHitTestSource(uint64_t id, XRSession* xr_session)
    : id_(id), xr_session_(xr_session) {}

uint64_t XRHitTestSource::id() const {
  return id_;
}

void XRHitTestSource::cancel(ExceptionState& exception_state) {
  if (!xr_session_->RemoveHitTestSource(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kCannotCancelHitTestSource);
  }
}

HeapVector<Member<XRHitTestResult>> XRHitTestSource::Results() {
  HeapVector<Member<XRHitTestResult>> results;

  for (const auto& result : last_frame_results_) {
    results.emplace_back(MakeGarbageCollected<XRHitTestResult>(*result));
  }

  return results;
}

void XRHitTestSource::Update(
    const Vector<device::mojom::blink::XRHitResultPtr>& hit_test_results) {
  last_frame_results_.clear();

  for (auto& result : hit_test_results) {
    last_frame_results_.push_back(
        std::make_unique<TransformationMatrix>(result->hit_matrix.matrix()));
  }
}
void XRHitTestSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
