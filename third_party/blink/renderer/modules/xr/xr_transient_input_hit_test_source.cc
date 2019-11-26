// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"

#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_result.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"

namespace blink {
XRTransientInputHitTestSource::XRTransientInputHitTestSource(uint64_t id)
    : id_(id) {}

uint64_t XRTransientInputHitTestSource::id() const {
  return id_;
}

void XRTransientInputHitTestSource::Update(
    const HashMap<uint32_t, Vector<device::mojom::blink::XRHitResultPtr>>&
        hit_test_results,
    XRInputSourceArray* input_source_array) {
  // TODO: Be smarter about the update - it's possible to add new resulst /
  // remove the ones that were removed & update the ones that are being changed.
  current_frame_results_.clear();

  // If we don't know anything about input sources, we won't be able to
  // construct any results so we are done (and current_frame_results_ should
  // stay empty).
  if (!input_source_array) {
    return;
  }

  for (const auto& source_id_and_results : hit_test_results) {
    XRInputSource* input_source =
        input_source_array->GetWithSourceId(source_id_and_results.key);
    // If the input source with the given ID could not be found, just skip
    // processing results for this input source.
    if (!input_source)
      continue;

    current_frame_results_.push_back(
        MakeGarbageCollected<XRTransientInputHitTestResult>(
            input_source, source_id_and_results.value));
  }
}

HeapVector<Member<XRTransientInputHitTestResult>>
XRTransientInputHitTestSource::Results() {
  return current_frame_results_;
}

void XRTransientInputHitTestSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_frame_results_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
